// Mini Moop - circuit-simulation synthesizer
// Copyright (C) 2026 Thierry Fraudet
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Fixed-size Modified Nodal Analysis circuit solver.
//
//  * resistors, capacitors (trapezoidal companion model), independent voltage
//    and current sources, Shockley diodes and BJTs (see Semiconductors.h)
//  * Newton-Raphson with SPICE-style junction limiting (pnjlim)
//  * DC operating point with source stepping fallback
//
// Node 0 is ground. Nodes are 1..NumNodes. The unknown vector holds the node
// voltages followed by the currents through the voltage sources.
//
// Topology is built once (allocation allowed) and then step() is real-time
// safe: matrices are fixed-size and no allocation happens.

#include "../components/Semiconductors.h"
#include "DenseLU.h"

#include <cassert>
#include <vector>

namespace mm
{

template <int NumNodes, int NumVSources = 0>
class MnaCircuit
{
public:
    static constexpr int N = NumNodes + NumVSources;

    struct Options
    {
        int    maxIterTransient = 20;
        int    maxIterDC        = 200;
        double reltol           = 1.0e-4;
        double vntol            = 1.0e-7;
        double gmin             = 1.0e-12;
    };

    // ------------------------------------------------------------------ build
    void addResistor (int a, int b, double r) { resistors.push_back ({ a, b, 1.0 / r }); dirty = true; }
    int addCapacitor (int a, int b, double c) { caps.push_back ({ a, b, c }); dirty = true; return int (caps.size()) - 1; }
    int addCurrentSource (int from, int to, double i) { isrcs.push_back ({ from, to, i }); return int (isrcs.size()) - 1; }
    int addVoltageSource (int pos, int neg, double v)
    {
        assert (int (vsrcs.size()) < NumVSources);
        vsrcs.push_back ({ pos, neg, v });
        dirty = true;
        return int (vsrcs.size()) - 1;
    }
    int addDiode (int anode, int cathode, const DiodeParams& p) { diodes.push_back ({ anode, cathode, p }); return int (diodes.size()) - 1; }
    int addBjt (int c, int b, int e, const BjtParams& p) { bjts.push_back ({ c, b, e, p }); return int (bjts.size()) - 1; }

    // -------------------------------------------------------------- settings
    void setTemperature (double kelvin) noexcept { vt = phys::thermalVoltage (kelvin); }
    void setTimeStep (double seconds) noexcept { h = seconds; dirty = true; }
    void setResistor (int idx, double r) noexcept { resistors[size_t (idx)].g = 1.0 / r; dirty = true; }
    void setCapacitor (int idx, double c) noexcept { caps[size_t (idx)].c = c; dirty = true; }
    void setVoltageSource (int idx, double v) noexcept { vsrcs[size_t (idx)].v = v; }
    void setCurrentSource (int idx, double i) noexcept { isrcs[size_t (idx)].i = i; }
    Options& options() noexcept { return opts; }

    // --------------------------------------------------------------- results
    double v (int node) const noexcept { return node == 0 ? 0.0 : x[size_t (node - 1)]; }
    double sourceCurrent (int idx) const noexcept { return x[size_t (NumNodes + idx)]; }
    int lastIterations() const noexcept { return iterations; }
    const Vector<N>& solution() const noexcept { return x; }

    // ------------------------------------------------------------- analyses
    /** DC operating point: capacitors open. Returns true when converged. */
    bool solveDC()
    {
        dcMode = true;
        dirty = true;
        bool ok = newton (opts.maxIterDC, 1.0);
        if (! ok)
        {
            // Source stepping
            x.fill (0.0);
            resetJunctions();
            ok = true;
            for (int s = 1; s <= 20 && ok; ++s)
                ok = newton (opts.maxIterDC, double (s) / 20.0);
        }
        for (auto& c : caps)
        {
            c.vPrev = v (c.a) - v (c.b);
            c.iPrev = 0.0;
        }
        dcMode = false;
        dirty = true;
        return ok;
    }

    /** Advance one time step h. Returns true when Newton converged. */
    bool step() noexcept
    {
        const bool ok = newton (opts.maxIterTransient, 1.0);
        for (auto& c : caps)
        {
            const double vNow = v (c.a) - v (c.b);
            const double geq = 2.0 * c.c / h;
            c.iPrev = geq * (vNow - c.vPrev) - c.iPrev;
            c.vPrev = vNow;
        }
        return ok;
    }

private:
    struct Res { int a, b; double g; };
    struct Cap { int a, b; double c; double vPrev = 0.0, iPrev = 0.0; };
    struct VSrc { int p, n; double v; };
    struct ISrc { int from, to; double i; };
    struct Dio { int a, k; DiodeParams p; double vOld = 0.0; };
    struct Bjt { int c, b, e; BjtParams p; double vbeOld = 0.0, vbcOld = 0.0; };

    static int row (int node) noexcept { return node - 1; }

    void addG (Matrix<N>& A, int a, int b, double g) const noexcept
    {
        if (a) A (row (a), row (a)) += g;
        if (b) A (row (b), row (b)) += g;
        if (a && b) { A (row (a), row (b)) -= g; A (row (b), row (a)) -= g; }
    }

    void addI (Vector<N>& rhs, int into, double i) const noexcept { if (into) rhs[size_t (row (into))] += i; }

    void buildLinear() noexcept
    {
        Alin.clear();
        for (const auto& r : resistors)
            addG (Alin, r.a, r.b, r.g);
        if (! dcMode)
            for (const auto& c : caps)
                addG (Alin, c.a, c.b, 2.0 * c.c / h);
        // gmin from every node to ground keeps floating nodes solvable
        for (int n = 1; n <= NumNodes; ++n)
            Alin (row (n), row (n)) += opts.gmin;
        for (size_t k = 0; k < vsrcs.size(); ++k)
        {
            const int j = NumNodes + int (k);
            const auto& s = vsrcs[k];
            if (s.p) { Alin (row (s.p), j) += 1.0; Alin (j, row (s.p)) += 1.0; }
            if (s.n) { Alin (row (s.n), j) -= 1.0; Alin (j, row (s.n)) -= 1.0; }
        }
        dirty = false;
    }

    void resetJunctions() noexcept
    {
        for (auto& d : diodes) d.vOld = 0.0;
        for (auto& q : bjts) { q.vbeOld = 0.0; q.vbcOld = 0.0; }
    }

    /** Stamp the linearised companion of a nonlinear branch current that
        flows *out of* node `from` through the device: I = I0 + sum g_k * v_k,
        where v_k are controlling node voltages. */
    bool newton (int maxIter, double sourceScale) noexcept
    {
        if (dirty)
            buildLinear();

        for (iterations = 0; iterations < maxIter; ++iterations)
        {
            Matrix<N> A = Alin;
            Vector<N> rhs {};

            if (! dcMode)
            {
                for (const auto& c : caps)
                {
                    const double ieq = 2.0 * c.c / h * c.vPrev + c.iPrev;
                    addI (rhs, c.a, ieq);
                    addI (rhs, c.b, -ieq);
                }
            }
            for (const auto& s : isrcs)
            {
                addI (rhs, s.from, -s.i * sourceScale);
                addI (rhs, s.to, s.i * sourceScale);
            }
            for (size_t k = 0; k < vsrcs.size(); ++k)
                rhs[size_t (NumNodes) + k] = vsrcs[k].v * sourceScale;

            bool limited = false;

            for (auto& d : diodes)
            {
                double vd = v (d.a) - v (d.k);
                const double nvt = d.p.n * vt;
                const double vl = phys::pnjlim (vd, d.vOld, nvt, phys::criticalVoltage (d.p.is, nvt));
                limited |= (vl != vd);
                vd = vl;
                d.vOld = vd;
                const auto e = evalDiode (d.p, vd, vt);
                const double g = e.g + opts.gmin;
                const double ieq = e.i + opts.gmin * vd - g * vd;
                addG (A, d.a, d.k, g);
                addI (rhs, d.a, -ieq);
                addI (rhs, d.k, ieq);
            }

            for (auto& q : bjts)
            {
                const double sgn = q.p.pnp ? -1.0 : 1.0;
                double vbe = sgn * (v (q.b) - v (q.e));
                double vbc = sgn * (v (q.b) - v (q.c));
                const double vcrit = phys::criticalVoltage (q.p.is, vt);
                const double vbeL = phys::pnjlim (vbe, q.vbeOld, vt, vcrit);
                const double vbcL = phys::pnjlim (vbc, q.vbcOld, vt, vcrit);
                limited |= (vbeL != vbe) || (vbcL != vbc);
                vbe = vbeL; vbc = vbcL;
                q.vbeOld = vbe; q.vbcOld = vbc;

                const auto e = evalBjt (q.p, vbe, vbc, vt, opts.gmin);
                // Currents into C and B terminals in circuit polarity
                const double ic = sgn * e.ic;
                const double ib = sgn * e.ib;
                // d(current)/d(node voltage): vbe = sgn (Vb - Ve), vbc = sgn (Vb - Vc)
                // dIc/dVb = sgn*sgn*(dic_dvbe + dic_dvbc) = dic_dvbe + dic_dvbc, etc.
                const double icb = e.dic_dvbe + e.dic_dvbc, ice = -e.dic_dvbe, icc = -e.dic_dvbc;
                const double ibb = e.dib_dvbe + e.dib_dvbc, ibe = -e.dib_dvbe, ibc = -e.dib_dvbc;
                const double vb = v (q.b), ve = v (q.e), vc = v (q.c);
                // Linearisation point in node voltages consistent with limited junctions
                // (use the limited junction voltages relative to the emitter).
                const double vbLin = ve + sgn * vbe;
                const double vcLin = vbLin - sgn * vbc;
                (void) vb; (void) vc;
                const double icEq = ic - (icb * vbLin + ice * ve + icc * vcLin);
                const double ibEq = ib - (ibb * vbLin + ibe * ve + ibc * vcLin);

                stampRow (A, rhs, q.c, q.b, q.e, q.c, icb, ice, icc, icEq);
                stampRow (A, rhs, q.b, q.b, q.e, q.c, ibb, ibe, ibc, ibEq);
                // emitter current into device = -(ic + ib)
                stampRow (A, rhs, q.e, q.b, q.e, q.c, -(icb + ibb), -(ice + ibe), -(icc + ibc), -(icEq + ibEq));
            }

            Vector<N> xNew = rhs;
            if (! luSolveInPlace (A, xNew))
                return false;

            bool converged = ! limited;
            for (int k = 0; k < N; ++k)
            {
                const double tol = opts.reltol * std::fmax (std::abs (xNew[size_t (k)]), std::abs (x[size_t (k)])) + opts.vntol;
                if (std::abs (xNew[size_t (k)] - x[size_t (k)]) > tol)
                    converged = false;
            }
            x = xNew;
            if (converged && iterations > 0)
            {
                ++iterations;
                return true;
            }
        }
        return false;
    }

    /** Row `node` gets: I_out = gB*Vb + gE*Ve + gC*Vc + ieq. */
    void stampRow (Matrix<N>& A, Vector<N>& rhs, int node, int b, int e, int c,
                   double gB, double gE, double gC, double ieq) const noexcept
    {
        if (! node) return;
        const int r = row (node);
        if (b) A (r, row (b)) += gB;
        if (e) A (r, row (e)) += gE;
        if (c) A (r, row (c)) += gC;
        rhs[size_t (r)] -= ieq;
    }

    std::vector<Res> resistors;
    std::vector<Cap> caps;
    std::vector<VSrc> vsrcs;
    std::vector<ISrc> isrcs;
    std::vector<Dio> diodes;
    std::vector<Bjt> bjts;

    Options opts;
    Matrix<N> Alin;
    Vector<N> x {};
    double h = 1.0 / 48000.0;
    double vt = phys::thermalVoltage (phys::roomTempK);
    bool dirty = true, dcMode = false;
    int iterations = 0;
};

} // namespace mm
