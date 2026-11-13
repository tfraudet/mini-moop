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
// Transistor ladder low-pass filter (4 poles), derived from the Ebers-Moll
// equations of the ladder's differential pairs:
//
//   C_n du_n/dt = (I_n / 2) * [ tanh((u_{n-1} + d_{n-1}) / 2Vt) - tanh((u_n + d_n) / 2Vt) ]
//
//   u_n  : differential voltage across ladder capacitor n
//   u_0  : differential input at the bottom pair = a*Vin - k*g(u_4) (emphasis feedback)
//   I_n  : tail current reaching stage n (alpha^n * I_f, base current losses)
//   d_n  : Vbe mismatch of pair n (Vt * ln(Is_a/Is_b))
//
// Integrated with the trapezoidal rule; the implicit system (including the
// zero-delay emphasis feedback) is solved with Newton-Raphson each sample.
//
// The top capacitor's voltage reaches the output through C5/C1 (a 15 Hz
// high-pass into R31/R38) and the discrete gain recovery amplifier Q5-Q8
// (FilterOutputStage, tabulated from ngspice): gain 68, asymmetric clipping.
// The emphasis is taken from that output's load (the junction of R3 1K and R7
// 180), through C10 10 uF and the EMPHASIS pot R14, to REGEN CAL R73, whose
// wiper is the base of Q30, the bottom pair's other input. The coupling and
// the clipping therefore sit inside the feedback loop: g() is the output
// stage's transfer normalised to unit slope, solved with the rest. C10 adds a
// second high-pass inside the loop, into the path's whole resistance: about
// 10 Hz at EMPHASIS 10 (R14 near 0), well under 1 Hz at 0. It only shows
// when the filter is made to sing at its lowest cutoffs.

#include "FilterOutputStage.h"
#include "../components/Passives.h"
#include "../components/Physics.h"
#include "../solver/DenseLU.h"

#include <array>
#include <cmath>
#include <utility>

namespace mm
{

struct LadderParams
{
    std::array<double, 4> c { 0.068e-6, 0.068e-6, 0.068e-6, 0.068e-6 }; // ladder capacitors (F)
    std::array<double, 5> offset {};  // Vbe mismatch of input pair + 4 ladder pairs (V)
    double beta       = 440.0;        // the TIS97's current gain (tools/spice/models.lib; its bin is 250 to 700)
    double inputGain  = 1.0;          // mixer node volts -> input pair: direct (base of Q29 over R54)

    // EMPHASIS feedback path (Fig. 9-11, front panel Fig. 9-16). The output's
    // load is R3 1K then R7 180 to the decoupled supply: their junction carries
    // R7 / (R3 + R7) of the output and goes through C10 10 uF to pin 7, the
    // wiper of R14 (50K reverse audio, used as a rheostat: wiper and terminal
    // 3). Terminal 3 comes back on pin 6 to one end of REGEN CAL R73 (1K); the
    // other end goes through R76 330 to the ladder's AC-grounded bias node, and
    // R73's wiper is Q30's base. The loop gain is the output stage's gain times
    //   R7 / (R3 + R7) * (R76 + t R73) / (R76 + R73 + R3 || R7 + R14)
    // 0.20 at EMPHASIS 0 (ngspice on the whole board, tools/spice/vcf_op.cir:
    // passband 56.7 for a stage gain of 68.0), 2.3 to 9.2 at 10 over the trim.
    double rEmphasisPot  = 50.0e3;    // R14
    double rLoadTop      = 180.0;     // R7
    double rLoadBottom   = 1.0e3;     // R3
    double rRegenFixed   = 330.0;     // R76
    double rRegenCal     = 1.0e3;     // R73
    double regenCal      = 0.73;      // R73's position: Voice's Regeneration Cal sets it (oscillation from EMPHASIS 7.5)
    double kOverride     = -1.0;      // >= 0: this loop gain whatever the EMPHASIS position (tests)
    double outputTrim = 1.0;          // the output stage's gain against its nominal (part tolerances)
    double rBias      = 47.0e3;       // R31/R38: the followers' base bias, with C5/C1 the coupling high-pass
    double cCoupling  = 0.22e-6;      // C5/C1
    double cEmphasis  = 10.0e-6;      // C10: the emphasis path's coupling
    bool   feedbackThroughOutputStage = true; // false: ideal linear DC-coupled feedback k*u4 (tests)
    double noiseRms   = 2.0e-6;       // input-referred transistor noise (V rms)

    static LadderParams drawn (Rng& rng, const LadderParams& nominal) noexcept
    {
        LadderParams p = nominal;
        for (auto& c : p.c)
            c = rng.tolerance (c, 0.10);          // 10 % film capacitors
        for (auto& d : p.offset)                  // "TIS97, Matched Pair" (Table 7-8): about 1 mV
            d = phys::thermalVoltage (298.15) * std::log (1.0 + 0.04 * rng.normal());
        p.beta = rng.tolerance (nominal.beta, 0.43, 0.59); // the gain bin, 250 to 700
        p.inputGain = rng.tolerance (nominal.inputGain, 0.02);
        p.outputTrim = rng.tolerance (nominal.outputTrim, 0.07);  // the TIS97's gain bin (sensitivity.cir)
        p.rEmphasisPot = rng.tolerance (nominal.rEmphasisPot, 0.20);
        p.rLoadTop = rng.tolerance (nominal.rLoadTop, 0.05);
        p.rLoadBottom = rng.tolerance (nominal.rLoadBottom, 0.05);
        p.rRegenFixed = rng.tolerance (nominal.rRegenFixed, 0.05);
        return p;
    }
};

class LadderFilter
{
public:
    void prepare (double sampleRate, uint64_t noiseSeed = 1) noexcept
    {
        fs = sampleRate;
        h = 1.0 / fs;
        rng.reseed (noiseSeed);
        reset();
        update();
        setEmphasis (emphasis); // C10's coefficients follow the rate
    }

    void setParams (const LadderParams& params) noexcept { p = params; update(); setEmphasis (emphasis); }
    const LadderParams& params() const noexcept { return p; }

    void setTemperature (double kelvin) noexcept { vt = phys::thermalVoltage (kelvin); update(); }

    /** Tail current of the bottom pair (from the filter expo converter). */
    void setCutoffCurrent (double amps) noexcept { iTail = std::fmax (amps, 1e-12); update(); }

    /** Emphasis pot position 0..1. */
    void setEmphasis (double pos) noexcept
    {
        emphasis = pos;
        k = p.kOverride >= 0.0 ? p.kOverride : loopGain (p, pos);
        // C10 into the path: R3 // R7, R14, R73 and R76 to the AC-grounded bias node
        const double source = p.rLoadTop * p.rLoadBottom / (p.rLoadTop + p.rLoadBottom);
        const double w = h / (p.cEmphasis * (source + emphasisOhms (p, pos) + p.rRegenCal + p.rRegenFixed));
        c10B = 1.0 / (1.0 + 0.5 * w);
        c10C = (1.0 - 0.5 * w) / (1.0 + 0.5 * w);
    }

    /** R14 between its wiper and terminal 3 (ohm) at a position 0..1: a
        reverse audio pot, all of its track at 0 and none at 10. */
    static double emphasisOhms (const LadderParams& p, double pos) noexcept
    {
        return p.rEmphasisPot * taper::audio (1.0 - std::fmin (std::fmax (pos, 0.0), 1.0));
    }

    /** The emphasis loop's gain for an EMPHASIS position 0..1: the feedback
        path's ratio times the output stage's small-signal gain. The ladder
        self-oscillates from about 4. */
    static double loopGain (const LadderParams& p, double pos) noexcept
    {
        const double tap = p.rLoadTop / (p.rLoadTop + p.rLoadBottom);
        const double source = p.rLoadTop * p.rLoadBottom / (p.rLoadTop + p.rLoadBottom);
        const double lower = p.rRegenFixed + p.regenCal * p.rRegenCal;
        const double divider = lower / (p.rRegenFixed + p.rRegenCal + source + emphasisOhms (p, pos));
        return stageGain * p.outputTrim * tap * divider;
    }

    void reset() noexcept
    {
        u.fill (0.0);
        fOld.fill (0.0);
        hpIn = hpOut = fbIn = fbOut = 0.0;
    }

    /** Cutoff (Hz) for a given tail current in the small-signal approximation. */
    double nominalCutoffForCurrent (double amps) const noexcept
    {
        return amps / (4.0 * p.c[0] * vt) / (2.0 * M_PI);
    }

    /** Process one sample. `vin` is the mixer output in volts. Returns volts. */
    double process (double vin) noexcept
    {
        const double inDiff = p.inputGain * vin + p.noiseRms * rng.normal();
        const double inv2vt = 1.0 / (2.0 * vt);
        const double hh = 0.5 * h;

        // Predictor: previous state + explicit step
        std::array<double, 4> x = u;
        for (int n = 0; n < 4; ++n)
            x[size_t (n)] += h * fOld[size_t (n)];

        std::array<double, 5> T {};
        std::array<double, 5> dT {};
        std::array<double, 4> f {};

        // Emphasis feedback through the coupling high-pass and the output stage,
        // as a function of x3 (and its slope), in units of x3.
        double hp = 0.0, fb = 0.0, dfb = 1.0;
        auto feedback = [&] (double x3) {
            if (! p.feedbackThroughOutputStage) { hp = x3; fb = x3; dfb = 1.0; return; }
            hp = hpB * (x3 - hpIn) + hpC * hpOut;
            double slope = 0.0;
            const double g = FilterOutputStage::transfer (-hp, slope) / stageGain;
            fb = c10B * (g - fbIn) + c10C * fbOut;    // through C10
            dfb = -slope / stageGain * hpB * c10B; // d(out)/d(x3) = slope * (-hpB), over stageGain: +hpB near 0
        };

        bool converged = false;
        for (int it = 0; it < maxIter; ++it)
        {
            feedback (x[3]);
            T[0] = std::tanh ((inDiff - k * fb + p.offset[0]) * inv2vt);
            dT[0] = (1.0 - T[0] * T[0]) * inv2vt;
            for (int n = 1; n <= 4; ++n)
            {
                T[size_t (n)] = std::tanh ((x[size_t (n - 1)] + p.offset[size_t (n)]) * inv2vt);
                dT[size_t (n)] = (1.0 - T[size_t (n)] * T[size_t (n)]) * inv2vt;
            }

            Matrix<4> J;
            Vector<4> r;
            for (int n = 0; n < 4; ++n)
            {
                const size_t sn = size_t (n);
                f[sn] = s[sn] * (T[sn] - T[sn + 1]);
                r[sn] = -(x[sn] - u[sn] - hh * (f[sn] + fOld[sn]));
                J (n, n) = 1.0 + hh * s[sn] * dT[sn + 1];
                if (n > 0)
                    J (n, n - 1) = -hh * s[sn] * dT[sn];
            }
            // Emphasis feedback: T0 depends on x3
            J (0, 3) += hh * s[0] * k * dfb * dT[0];

            if (! luSolveInPlace (J, r))
                break;

            double maxStep = 0.0;
            std::array<double, 4> d {};
            for (int n = 0; n < 4; ++n)
            {
                // Damp very large steps (cannot exceed a few Vt per iteration)
                d[size_t (n)] = std::fmax (-0.2, std::fmin (0.2, r[size_t (n)]));
                x[size_t (n)] += d[size_t (n)];
                maxStep = std::fmax (maxStep, std::abs (d[size_t (n)]));
            }
            if (maxStep < 1.0e-8)
            {
                // Converged: update the tanh terms to first order instead of
                // re-evaluating them (error O(step^2), far below 1e-12).
                T[0] -= dT[0] * k * dfb * d[3];
                for (int n = 1; n <= 4; ++n)
                    T[size_t (n)] += dT[size_t (n)] * d[size_t (n - 1)];
                converged = true;
                break;
            }
        }

        if (! converged)
        {
            feedback (x[3]);
            T[0] = std::tanh ((inDiff - k * fb + p.offset[0]) * inv2vt);
            for (int n = 1; n <= 4; ++n)
                T[size_t (n)] = std::tanh ((x[size_t (n - 1)] + p.offset[size_t (n)]) * inv2vt);
        }
        // Derivative at the accepted solution, for the next trapezoidal step
        for (int n = 0; n < 4; ++n)
            fOld[size_t (n)] = s[size_t (n)] * (T[size_t (n)] - T[size_t (n + 1)]);

        u = x;
        if (! std::isfinite (u[3]))
        {
            reset();
            return 0.0;
        }

        // The output: the top capacitor through the coupling high-pass into the
        // output stage (vd = -u3: the top capacitor is inverted against the input).
        hp = hpB * (u[3] - hpIn) + hpC * hpOut;
        hpIn = u[3];
        hpOut = hp;
        double slope = 0.0;
        const double out = FilterOutputStage::transfer (-hp, slope);
        if (p.feedbackThroughOutputStage)
        {
            // C10's state, at the accepted solution
            const double g = out / stageGain;
            fbOut = c10B * (g - fbIn) + c10C * fbOut;
            fbIn = g;
        }
        return p.outputTrim * out;
    }

    double stageVoltage (int n) const noexcept { return u[size_t (n)]; }
    void setMaxIterations (int n) noexcept { maxIter = n; }

private:
    void update() noexcept
    {
        // C5/C1 into R31/R38, trapezoidal: y_n = b (x_n - x_n-1) + c y_n-1
        const double w = h / (p.rBias * p.cCoupling);
        hpB = 1.0 / (1.0 + 0.5 * w);
        hpC = (1.0 - 0.5 * w) / (1.0 + 0.5 * w);
        const double alpha = p.beta / (p.beta + 1.0);
        double a = alpha;
        for (int n = 0; n < 4; ++n)
        {
            s[size_t (n)] = a * iTail / (2.0 * p.c[size_t (n)]);
            a *= alpha;
        }
    }

    static constexpr double stageGain = -FilterOutputStage::smallSignalGain; // 68.0: g() has unit slope
    LadderParams p;
    double hpB = 1.0, hpC = 1.0, hpIn = 0.0, hpOut = 0.0;
    double c10B = 1.0, c10C = 1.0, fbIn = 0.0, fbOut = 0.0; // C10, trapezoidal
    Rng rng;
    double fs = 48000.0, h = 1.0 / 48000.0;
    double vt = phys::thermalVoltage (298.15);
    double iTail = 1.0e-5, k = 0.0, emphasis = 0.0;
    int maxIter = 8;
    std::array<double, 4> s {}, u {}, fOld {};
};

} // namespace mm
