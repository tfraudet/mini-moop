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
//   u_0  : differential input at the bottom pair = a*Vin - k*u_4 (emphasis feedback)
//   I_n  : tail current reaching stage n (alpha^n * I_f, base current losses)
//   d_n  : Vbe mismatch of pair n (Vt * ln(Is_a/Is_b))
//
// Integrated with the trapezoidal rule; the implicit system (including the
// zero-delay emphasis feedback) is solved with Newton-Raphson each sample.
// The differential output is converted to single-ended by an op-amp stage.

#include "../components/OpAmp.h"
#include "../components/Passives.h"
#include "../components/Physics.h"
#include "../solver/DenseLU.h"

#include <array>
#include <cmath>

namespace mm
{

struct LadderParams
{
    std::array<double, 4> c { 0.068e-6, 0.068e-6, 0.068e-6, 0.068e-6 }; // ladder capacitors (F)
    std::array<double, 5> offset {};  // Vbe mismatch of input pair + 4 ladder pairs (V)
    double beta       = 200.0;        // transistor current gain
    double inputGain  = 0.012;        // mixer volts -> input pair differential volts
    double kMax       = 14.2;         // emphasis feedback at full rotation: oscillation (k = 4.0) from 7.5/10

    // EMPHASIS feedback path (board 4 pins 7 -> 6, front panel Fig. 9-16): the
    // filter output, through C10 10 uF, feeds the wiper of R14 (50k reverse
    // audio taper, wired as a rheostat: wiper and terminal 3, terminal 1 open);
    // terminal 3 returns through R76 330 and the REGEN CAL trimmer R73 to the
    // bottom pair. The feedback is the divider of R14's track (plus the output
    // node's source resistance) against the bottom pair side (shunt, assumed).
    double rEmphasisPot    = 50.0e3;  // R14
    double rEmphasisSource = 180.0;   // output node (R7)
    double rEmphasisShunt  = 800.0;   // R76 + part of R73 || pair input (assumed)
    double outputGain = 18.0;         // output differential amp gain
    double noiseRms   = 2.0e-6;       // input-referred transistor noise (V rms)

    static LadderParams drawn (Rng& rng, const LadderParams& nominal) noexcept
    {
        LadderParams p = nominal;
        for (auto& c : p.c)
            c = rng.tolerance (c, 0.10);          // 10 % film capacitors
        for (auto& d : p.offset)                  // unmatched small-signal pairs: Is spread ~4 %
            d = phys::thermalVoltage (298.15) * std::log (1.0 + 0.04 * rng.normal());
        p.beta = rng.tolerance (nominal.beta, 0.5);
        p.inputGain = rng.tolerance (nominal.inputGain, 0.02);
        p.outputGain = rng.tolerance (nominal.outputGain, 0.02);
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
        outAmp.prepare (OpAmpParams::lm301(), fs, p.outputGain);
        reset();
        update();
    }

    void setParams (const LadderParams& params) noexcept { p = params; update(); }
    const LadderParams& params() const noexcept { return p; }

    void setTemperature (double kelvin) noexcept { vt = phys::thermalVoltage (kelvin); update(); }

    /** Tail current of the bottom pair (from the filter expo converter). */
    void setCutoffCurrent (double amps) noexcept { iTail = std::fmax (amps, 1e-12); update(); }

    /** Emphasis pot position 0..1. */
    void setEmphasis (double pos) noexcept { k = p.kMax * emphasisLaw (p, pos); }

    /** Feedback for an EMPHASIS position, relative to full rotation (0..1):
        R14 track between wiper and terminal 3 = Rpot * audio(1 - pos) for a
        reverse-audio pot, in a divider against the shunt resistance. */
    static double emphasisLaw (const LadderParams& p, double pos) noexcept
    {
        auto feedback = [&p] (double a) {
            const double series = p.rEmphasisPot * taper::audio (1.0 - a) + p.rEmphasisSource;
            return p.rEmphasisShunt / (p.rEmphasisShunt + series);
        };
        const double a = std::fmin (std::fmax (pos, 0.0), 1.0);
        return feedback (a) / feedback (1.0);
    }

    void reset() noexcept
    {
        u.fill (0.0);
        fOld.fill (0.0);
        outAmp.reset();
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

        bool converged = false;
        for (int it = 0; it < maxIter; ++it)
        {
            T[0] = std::tanh ((inDiff - k * x[3] + p.offset[0]) * inv2vt);
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
            J (0, 3) += hh * s[0] * k * dT[0];

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
                T[0] -= dT[0] * k * d[3];
                for (int n = 1; n <= 4; ++n)
                    T[size_t (n)] += dT[size_t (n)] * d[size_t (n - 1)];
                converged = true;
                break;
            }
        }

        if (! converged)
        {
            T[0] = std::tanh ((inDiff - k * x[3] + p.offset[0]) * inv2vt);
            for (int n = 1; n <= 4; ++n)
                T[size_t (n)] = std::tanh ((x[size_t (n - 1)] + p.offset[size_t (n)]) * inv2vt);
        }
        // Derivative at the accepted solution, for the next trapezoidal step
        for (int n = 0; n < 4; ++n)
            fOld[size_t (n)] = s[size_t (n)] * (T[size_t (n)] - T[size_t (n + 1)]);

        u = x;
        if (! std::isfinite (u[3]))
            reset();

        return outAmp.process (p.outputGain * u[3]);
    }

    double stageVoltage (int n) const noexcept { return u[size_t (n)]; }
    void setMaxIterations (int n) noexcept { maxIter = n; }

private:
    void update() noexcept
    {
        const double alpha = p.beta / (p.beta + 1.0);
        double a = alpha;
        for (int n = 0; n < 4; ++n)
        {
            s[size_t (n)] = a * iTail / (2.0 * p.c[size_t (n)]);
            a *= alpha;
        }
    }

    LadderParams p;
    OpAmpStage outAmp;
    Rng rng;
    double fs = 48000.0, h = 1.0 / 48000.0;
    double vt = phys::thermalVoltage (298.15);
    double iTail = 1.0e-5, k = 0.0;
    int maxIter = 8;
    std::array<double, 4> s {}, u {}, fOld {};
};

} // namespace mm
