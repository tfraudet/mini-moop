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
// Voltage-controlled amplifier (board 4, circuit 7, Fig. 9-11): differential
// pair whose tail current is set by the loudness contour through the V->I
// transistor Q18, followed by a differential to single-ended stage.
//
//   Iout_diff = Itail(Vcontour) * tanh((Vin*a + d) / 2Vt)
//
// Itail: the contour drives Q18's base through R59 68K; its emitter sits on
// R37 6.8K to ground and R43 270K to -10 V, its collector reaches the pair's
// emitters through R35 1K. Q18 is solved with the Ebers-Moll equations (the
// same BJT model as everywhere else) over the contour's range when the
// parameters are set, and read from that table per sample. The result is the
// circuit's law, checked against ngspice (tests/test_vca.cpp):
//   - Q18 is off below about 0.25 V (the contour rests at -0.5 V, so the VCA
//     is fully closed between notes, apart from the bleed below);
//   - above that the tail current is linear in the contour (R37 || R43);
//   - from about 4.3 V Q18 saturates against R35: the gain stops rising and
//     falls slightly. The attack's peak (5.75 V) and the top of the decay,
//     down to about 4.3 V, all play at full gain.
//
// Also modelled: input pair saturation (soft clipping of hot signals), pair
// mismatch d and control-voltage feedthrough (the VCA "thump" when the
// contour opens), and output stage limits.

#include "../components/OpAmp.h"
#include "../components/Passives.h"
#include "../components/Physics.h"
#include "../components/Semiconductors.h"
#include "../solver/DenseLU.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mm
{

struct VcaParams
{
    double inputGain   = 0.010;   // filter output volts -> pair differential volts: R2 82K / R34 820 (Fig. 9-11)
    double offset      = 0.0;     // pair Vbe mismatch (V)
    double feedthrough = 0.004;   // common-mode rejection error of the output amp
    double iMax        = 1.0e-3;  // tail current giving unity gain; the circuit's tail at a 5 V contour is scaled to it
    double rLoad       = 10.0e3;  // collector load resistors
    double ampGain     = 0.5;     // differential amp gain (unity small-signal at full contour)
    double bleed       = 1.0e-4;  // residual gain with the VCA closed, of the full-contour gain (80 dB range)

    // Q18, the contour's V->I converter (Fig. 9-11)
    double r59 = 68.0e3;          // R59 68K: contour -> Q18 base
    double r37 = 6.8e3;           // R37 6.8K: emitter to ground
    double r43 = 270.0e3;         // R43 270K: emitter to -10 V
    double r35 = 1.0e3;           // R35 1K: collector to the pair's emitters
    double vPairEmitters = 4.10;  // the pair's emitter node: R36's bias node less a Vbe (ngspice operating point)
    BjtParams q18 { 5.0e-15, 420.0, 4.0, 100.0, false }; // TIS97

    static VcaParams drawn (Rng& rng, const VcaParams& nominal) noexcept
    {
        VcaParams p = nominal;
        p.offset = phys::thermalVoltage (298.15) * std::log (1.0 + 0.03 * rng.normal());
        p.feedthrough = nominal.feedthrough * (0.5 + rng.uniform());
        p.inputGain = rng.tolerance (nominal.inputGain, 0.05);
        p.bleed = nominal.bleed * (0.5 + rng.uniform());
        p.r37 = rng.tolerance (nominal.r37, 0.05);
        p.r43 = rng.tolerance (nominal.r43, 0.05);
        p.r59 = rng.tolerance (nominal.r59, 0.05);
        return p;
    }
};

class Vca
{
public:
    static constexpr double tableMin = -1.0, tableMax = 10.0, tableStep = 0.005;
    static constexpr int tableSize = int ((tableMax - tableMin) / tableStep) + 2;

    void prepare (double sampleRate) noexcept
    {
        amp.prepare (OpAmpParams::ua741(), sampleRate, 4.0);
        amp.reset();
    }

    void setParams (const VcaParams& params) noexcept
    {
        p = params;
        buildTailTable();
    }
    void setTemperature (double kelvin) noexcept { inv2vt = 1.0 / (2.0 * phys::thermalVoltage (kelvin)); }

    /** Q18's collector current (A) for a loudness contour voltage (V), as solved
        from the circuit (no bleed, no scaling). */
    double tailCurrent (double contourVolts) const noexcept
    {
        const double x = (std::fmin (std::fmax (contourVolts, tableMin), tableMax) - tableMin) / tableStep;
        const int i = std::min (int (x), tableSize - 2);
        const double f = x - double (i);
        return tail[size_t (i)] + f * (tail[size_t (i + 1)] - tail[size_t (i)]);
    }

    /** `vin` = filter output (V), `contourVolts` = loudness contour output (V). */
    double process (double vin, double contourVolts) noexcept
    {
        const double iTail = p.iMax * (p.bleed + tailCurrent (contourVolts) * tailScale);
        const double diff = iTail * std::tanh ((vin * p.inputGain + p.offset) * inv2vt);
        // Differential output plus common-mode leak of the tail current.
        const double v = p.rLoad * (diff + p.feedthrough * iTail);
        return amp.process (p.ampGain * v);
    }

private:
    /** Solves Q18 (base, emitter and collector nodes) by Newton's method at each
        contour voltage of the table, each from the previous solution. */
    void buildTailTable() noexcept
    {
        const double vt = phys::thermalVoltage (298.15);
        const double vNeg = -10.0;
        double vb = tableMin, ve = -0.4, vc = p.vPairEmitters;
        for (int n = 0; n < tableSize; ++n)
        {
            const double vcont = tableMin + n * tableStep;
            for (int it = 0; it < 200; ++it)
            {
                const BjtEval e = evalBjt (p.q18, vb - ve, vb - vc, vt);
                const double ie = e.ic + e.ib;
                Vector<3> r { -((vcont - vb) / p.r59 - e.ib),
                              -(ie - ve / p.r37 - (ve - vNeg) / p.r43),
                              -((p.vPairEmitters - vc) / p.r35 - e.ic) };
                const double die_dvbe = e.dic_dvbe + e.dib_dvbe, die_dvbc = e.dic_dvbc + e.dib_dvbc;
                Matrix<3> J;
                J (0, 0) = -1.0 / p.r59 - e.dib_dvbe - e.dib_dvbc; J (0, 1) = e.dib_dvbe; J (0, 2) = e.dib_dvbc;
                J (1, 0) = die_dvbe + die_dvbc; J (1, 1) = -die_dvbe - 1.0 / p.r37 - 1.0 / p.r43; J (1, 2) = -die_dvbc;
                J (2, 0) = -(e.dic_dvbe + e.dic_dvbc); J (2, 1) = e.dic_dvbe; J (2, 2) = -1.0 / p.r35 + e.dic_dvbc;
                if (! luSolveInPlace (J, r))
                    break;
                double step = 0.0;
                for (auto& d : r)
                {
                    d = std::fmax (-0.05, std::fmin (0.05, d)); // a few Vt at most per iteration
                    step = std::fmax (step, std::abs (d));
                }
                vb += r[0]; ve += r[1]; vc += r[2];
                if (step < 1.0e-10)
                    break;
            }
            tail[size_t (n)] = std::fmax (0.0, evalBjt (p.q18, vb - ve, vb - vc, vt).ic);
        }
        // Scale so that a 5 V contour (the sustained top of the loudness contour)
        // gives iMax, i.e. the same full-contour gain as before.
        tailScale = 1.0 / std::fmax (tailCurrent (5.0), 1.0e-9);
    }

    VcaParams p;
    OpAmpStage amp;
    std::array<double, size_t (tableSize)> tail {};
    double tailScale = 1.0;
    double inv2vt = 1.0 / (2.0 * phys::thermalVoltage (298.15));
};

} // namespace mm
