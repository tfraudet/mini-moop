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
// Voltage controlled amplifier (board 4, circuit 7, Fig. 9-11; service manual
// 2.9): two transconductance multipliers in cascade and a differential to
// single-ended output stage, all discrete.
//
//   First VCA   Q16 / Q15. The filter's output reaches Q16's base through R2
//               82K and C6 .33 uF, across R34 820 (1/101). The pair's tail is
//               Q18, driven by the loudness contour: Itail(Vcontour). Loads
//               R19 / R23 100 and 1ST VCA BAL R14 25:
//                 v2 = RL1 * I1 * tanh (v1 / 2Vt)
//   Second VCA  Q14 / Q13 on the first pair's collectors. Its tail Q21 is set
//               by the EXT. LOUDNESS jack, whose shorting contact holds it on
//               (+10 V through R56 and R51 33K): 1.37 mA. Loads R18 / R21 330
//               and 2ND VCA BAL R12 100:
//                 v3 = RL2 * I2 * tanh (v2 / 2Vt)
//   Output      Q12 / Q17 (2N4058) on the second pair's collectors, emitters
//               through R8 / R28 8.2 to the current source Q1 (9.7 mA). Q17's
//               collector on R29 680 is the output, ahead of C2 10 uF, R75 1K
//               and the VOLUME pot:
//                 v3 = 2Vt atanh (x) + RE I3 x,   out = Ro (I3 / 2) x
//               It can only swing by Ro I3 / 2, about 2.8 V each way: the
//               instrument's output clips softly there.
//
// Q18's law: the contour drives its base through R59 68K; its emitter sits on
// R37 6.8K to ground and R43 270K to -10 V, its collector reaches the pair's
// emitters through R35 1K. Q18 is solved with the Ebers-Moll equations over
// the contour's range when the parameters are set. It is off below about
// 0.25 V (the contour rests at -0.5 V: the VCA is closed between notes), linear
// in the contour above, and saturates against R35 from about 4.3 V.
//
// Checked against ngspice on the netlist transcribed from Fig. 9-11
// (tools/spice/board4-vca.lib, vca_op.cir, vca_large.cir): gain 1.97 from the
// filter's output at a 5 V contour, the tail current against the contour, the
// clipping levels; and against the service manual: +2.00 V on EXT. LOUDNESS
// lowers the output by 10 +/- 3 dB (5.23; the netlist: 9.0 dB).
//
// The A-440 (circuit 13) comes in through R40 and C8 on Q14's collector, the
// output pair's base: after both VCAs, so it sounds with no key down, goes
// through the output stage and its clipping, and reaches the VOLUME pot, the
// OUTPUT switch and the feedback patch like the instrument's own signal.
// R40 is 1K on the original board, 10K with Folkman's 94-002 kit, 2.2K on Fig.
// 9-11 and in Table 7-8; ngspice (tools/spice/a440_level.cir) puts the A-440
// at the main output at 0, -9 and -1.3 dB for them: only the kit's 10K meets
// the service manual's "-8 +/- 2dB" (5.7), and is the value used.
//
// 1ST VCA BAL nulls the first pair's imbalance, which otherwise puts a step
// proportional to the tail current on the output each time the contour opens
// (the "thump"); what the technician's null leaves is `balance1`.

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
    // Input (Fig. 9-11)
    double r2  = 82.0e3;          // R2: from the filter's output
    double r34 = 820.0;           // R34: across the first pair's bases
    double c6  = 0.33e-6;         // C6: input coupling

    // First VCA
    double rLoad1   = 112.5;      // R19 / R23 100 plus half of R14 25
    double offset1  = 0.0;        // the pair's Vbe mismatch (V)
    double balance1 = 0.0;        // load imbalance left after 1ST VCA BAL, ohm (as a difference of the two loads)
    double bleed    = 1.0e-4;     // residual tail with Q18 off, of the full tail ("dynamic range 80 dB")

    // Q18, the contour's V->I converter
    double r59 = 68.0e3;          // R59 68K: contour -> Q18 base
    double r37 = 6.8e3;           // R37 6.8K: emitter to ground
    double r43 = 270.0e3;         // R43 270K: emitter to -10 V
    double r35 = 1.0e3;           // R35 1K: collector to the pair's emitters
    double vPairEmitters = 4.10;  // the first pair's emitters: the R22 / R36 bias node (4.72 V) less a Vbe (ngspice)
    BjtParams q18 { 1.0e-14, 440.0, 4.0, 100.0, false }; // TIS97 (tools/spice/models.lib)

    // Second VCA
    double rLoad2 = 334.0;        // R18 / R21 330 plus half of R12 100, beside the output pair's bases (2.8K)
    double i2     = 1.374e-3;     // Q21's collector current with the EXT. LOUDNESS jack unplugged (ngspice)

    // Output stage
    double rEmitter = 9.2;        // R8 / R28 8.2, plus each transistor's own emitter and base resistance
    double i3       = 9.71e-3;    // current source Q1: (10 V - the R10 / R11 node - Vbe) / R9 62 (ngspice)
    double rOut     = 583.0;      // R29 680 beside R75 1K + VOLUME 5K and Q17's output resistance
    double cOut     = 10.0e-6;    // C2
    double rAfter   = 6.0e3;      // R75 1K + R20 5K: what C2 drives

    // The A-440's way in: R40 and C8 into the R18 330 side of the second load
    double r40      = 10.0e3;     // see above: the 94-002 kit's value
    double c8       = 0.1e-6;
    double rA440Node = 330.0;     // R18

    static VcaParams drawn (Rng& rng, const VcaParams& nominal) noexcept
    {
        VcaParams p = nominal;
        p.offset1 = phys::thermalVoltage (298.15) * std::log (1.0 + 0.03 * rng.normal());
        // The technician nulls the feedthrough to "-55dB maximum" (5.25): a few
        // microvolts on the pair's loads.
        p.balance1 = 0.007 * rng.normal();
        p.bleed = nominal.bleed * (0.5 + rng.uniform());
        p.r2 = rng.tolerance (nominal.r2, 0.05);
        p.r34 = rng.tolerance (nominal.r34, 0.05);
        p.r37 = rng.tolerance (nominal.r37, 0.05);
        p.r43 = rng.tolerance (nominal.r43, 0.05);
        p.r59 = rng.tolerance (nominal.r59, 0.05);
        p.rLoad1 = rng.tolerance (nominal.rLoad1, 0.04);
        p.rLoad2 = rng.tolerance (nominal.rLoad2, 0.04);
        p.i2 = rng.tolerance (nominal.i2, 0.08);     // R42, R51, R56 and Q21's gain
        p.i3 = rng.tolerance (nominal.i3, 0.10);     // R9, R10 and Q1's Vbe
        p.rOut = rng.tolerance (nominal.rOut, 0.04);
        p.q18.bf = rng.tolerance (nominal.q18.bf, 0.4); // the TIS97's gain bin, 250 to 700
        return p;
    }
};

class Vca
{
public:
    static constexpr double tableMin = -1.0, tableMax = 10.0, tableStep = 0.005;
    static constexpr int tableSize = int ((tableMax - tableMin) / tableStep) + 2;
    static constexpr double outMax = 0.6, outStep = 0.001; // the output pair's table: v3 from -0.6 to 0.6 V
    static constexpr int outSize = int (2.0 * outMax / outStep) + 2;

    void prepare (double sampleRate) noexcept
    {
        h = 1.0 / sampleRate;
        update();
        reset();
    }

    void reset() noexcept { inPrev = inHp = outPrev = outHp = refPrev = refHp = 0.0; }

    void setParams (const VcaParams& params) noexcept
    {
        p = params;
        buildTailTable();
        buildOutputTable();
        update();
    }
    const VcaParams& params() const noexcept { return p; }
    void setTemperature (double kelvin) noexcept { inv2vt = 1.0 / (2.0 * phys::thermalVoltage (kelvin)); }

    /** Q18's collector current (A) for a loudness contour voltage (V), as solved
        from the circuit. */
    double tailCurrent (double contourVolts) const noexcept
    {
        const double x = (std::fmin (std::fmax (contourVolts, tableMin), tableMax) - tableMin) / tableStep;
        const int i = std::min (int (x), tableSize - 2);
        const double f = x - double (i);
        return tail[size_t (i)] + f * (tail[size_t (i + 1)] - tail[size_t (i)]);
    }

    /** Small-signal gain from the filter's output to Q17's collector for a
        contour voltage (25 C). */
    double smallSignalGain (double contourVolts) const noexcept
    {
        const double vt2 = 2.0 * phys::thermalVoltage (298.15);
        return inputRatio() * (p.rLoad1 * tailCurrent (contourVolts) / vt2) * (p.rLoad2 * p.i2 / vt2)
             * (p.rOut * 0.5 * p.i3 / (vt2 + p.rEmitter * p.i3));
    }

    /** The output's swing each way (V): the output pair fully switched. */
    double swing() const noexcept { return p.rOut * 0.5 * p.i3; }

    /** `vin`: the filter's output (V); `contourVolts`: the loudness contour;
        `a440`: the A-440 oscillator's output (V, Q4's emitter), 0 when off.
        Returns the AC voltage of Q17's collector after C2, as R75 and the
        VOLUME pot see it. */
    double process (double vin, double contourVolts, double a440 = 0.0) noexcept
    {
        // the A-440 through C8 into R40 and the load: a voltage on one of the
        // output pair's bases
        refHp = kRef * (refHp + a440 - refPrev);
        refPrev = a440;
        // C6 into R2 + R34
        inHp = kIn * (inHp + vin - inPrev);
        inPrev = vin;
        const double v1 = inHp * inputRatio();
        const double i1 = std::fmax (tailCurrent (contourVolts), p.bleed * tailFull);
        const double v2 = i1 * (p.rLoad1 * std::tanh ((v1 + p.offset1) * inv2vt) + p.balance1
                                - p.rLoad1 * tanhOffset1);
        const double v3 = p.rLoad2 * p.i2 * std::tanh (v2 * inv2vt) + refHp * p.rA440Node / (p.r40 + p.rA440Node);
        const double out = swing() * outputPair (v3);
        // C2 into R75 + VOLUME
        outHp = kOut * (outHp + out - outPrev);
        outPrev = out;
        return outHp;
    }

private:
    double inputRatio() const noexcept { return p.r34 / (p.r2 + p.r34); }

    /** x = (I17 - I12) / I3 for the voltage between the output pair's bases. */
    double outputPair (double v3) const noexcept
    {
        const double x = (std::fmin (std::fmax (v3, -outMax), outMax) + outMax) / outStep;
        const int i = std::min (int (x), outSize - 2);
        const double f = x - double (i);
        return pair[size_t (i)] + f * (pair[size_t (i + 1)] - pair[size_t (i)]);
    }

    void update() noexcept
    {
        auto highPass = [this] (double rc) { return rc / (rc + h); };
        kIn = highPass (p.c6 * (p.r2 + p.r34));
        kOut = highPass (p.cOut * p.rAfter);
        kRef = highPass (p.c8 * (p.r40 + p.rA440Node));
        tanhOffset1 = std::tanh (p.offset1 / (2.0 * phys::thermalVoltage (298.15))); // what 1ST VCA BAL cancels, set at 25 C
    }

    /** Solves the emitter-degenerated pair v3 = 2Vt atanh (x) + RE I3 x. */
    void buildOutputTable() noexcept
    {
        const double vt2 = 2.0 * phys::thermalVoltage (298.15);
        const double k = p.rEmitter * p.i3;
        double u = 0.0;
        for (int n = 0; n < outSize; ++n)
        {
            const double v3 = -outMax + n * outStep;
            if (n == 0) u = v3 / (vt2 + k);
            for (int it = 0; it < 60; ++it)
            {
                const double th = std::tanh (u);
                const double g = vt2 * u + k * th - v3;
                const double step = g / (vt2 + k * (1.0 - th * th));
                u -= step;
                if (std::abs (step) < 1.0e-12)
                    break;
            }
            pair[size_t (n)] = std::tanh (u);
        }
    }

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
        tailFull = tailCurrent (5.0);
    }

    VcaParams p;
    std::array<double, size_t (tableSize)> tail {};
    std::array<double, size_t (outSize)> pair {};
    double tailFull = 5.5e-4, tanhOffset1 = 0.0;
    double h = 1.0 / 48000.0, kIn = 1.0, kOut = 1.0, kRef = 1.0;
    double inPrev = 0.0, inHp = 0.0, outPrev = 0.0, outHp = 0.0, refPrev = 0.0, refHp = 0.0;
    double inv2vt = 1.0 / (2.0 * phys::thermalVoltage (298.15));
};

} // namespace mm
