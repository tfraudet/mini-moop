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
// The emphasis is taken from that output (R3/R7, C10, EMPHASIS, R73), so the
// coupling and the clipping sit inside the feedback loop: g() is the output
// stage's transfer normalised to unit slope, solved with the rest.

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
    double beta       = 200.0;        // transistor current gain
    double inputGain  = 1.0;          // mixer node volts -> input pair: direct (base of Q29 over R54)
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
    double outputTrim = 1.0;          // the output stage's gain against its nominal (part tolerances)
    double rBias      = 47.0e3;       // R31/R38: the followers' base bias, with C5/C1 the coupling high-pass
    double cCoupling  = 0.22e-6;      // C5/C1
    bool   feedbackThroughOutputStage = true; // false: ideal linear DC-coupled feedback k*u4 (tests)
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
        p.outputTrim = rng.tolerance (nominal.outputTrim, 0.03);
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
    }

    void setParams (const LadderParams& params) noexcept { p = params; update(); }
    const LadderParams& params() const noexcept { return p; }

    void setTemperature (double kelvin) noexcept { vt = phys::thermalVoltage (kelvin); update(); }

    /** Tail current of the bottom pair (from the filter expo converter). */
    void setCutoffCurrent (double amps) noexcept { iTail = std::fmax (amps, 1e-12); update(); }

    /** Emphasis pot position 0..1. */
    void setEmphasis (double pos) noexcept { k = p.kMax * emphasisLaw (p, pos); }

    /** Feedback through the drawn EMPHASIS pot, relative to full rotation
        (0..1): R14 track between wiper and terminal 3 = Rpot * audio(1 - pos)
        for a reverse-audio pot, in a divider against the shunt resistance. */
    static double emphasisLawDrawn (const LadderParams& p, double pos) noexcept
    {
        auto feedback = [&p] (double a) {
            const double series = p.rEmphasisPot * taper::audio (1.0 - a) + p.rEmphasisSource;
            return p.rEmphasisShunt / (p.rEmphasisShunt + series);
        };
        const double a = std::fmin (std::fmax (pos, 0.0), 1.0);
        return feedback (a) / feedback (1.0);
    }

    /** Feedback for an EMPHASIS position, relative to full rotation (0..1).
        Up to 8.5, the loop gain at which the passband falls as the CA-72
        project's hardware reference's does (its sessions E and J: -2.25, -9.53,
        -10.43, -11.92, -12.50 and -14.67 dB at 2.5, 5, 6, 7, 7.5 and 8.5,
        against EMPHASIS 0): k = (1 + k0) 10^(-drop/20) - 1, k0 the residual
        feedback at 0; straight in k's logarithm between them. From 8.5 to 10
        the drawn law's shape, scaled to meet it. */
    static double emphasisLaw (const LadderParams& p, double pos) noexcept
    {
        static constexpr std::array<std::pair<double, double>, 6> drop { {
            { 0.25, -2.25 }, { 0.5, -9.53 }, { 0.6, -10.43 }, { 0.7, -11.92 }, { 0.75, -12.50 }, { 0.85, -14.67 } } };
        const double a = std::fmin (std::fmax (pos, 0.0), 1.0);
        if (p.kMax <= 0.0)
            return emphasisLawDrawn (p, a); // no feedback at all (tests)
        const double k0 = p.kMax * emphasisLawDrawn (p, 0.0);
        auto kAt = [k0] (double dB) { return (1.0 + k0) * std::pow (10.0, -dB / 20.0) - 1.0; };
        double prevPos = 0.0, prevK = k0;
        for (const auto& [x, dB] : drop)
        {
            const double kx = kAt (dB);
            if (a <= x)
                return prevK * std::pow (kx / prevK, (a - prevPos) / (x - prevPos)) / p.kMax;
            prevPos = x;
            prevK = kx;
        }
        // 8.5 .. 10: the drawn law's shape from k(8.5) to kMax
        const double l0 = emphasisLawDrawn (p, prevPos);
        const double f = (emphasisLawDrawn (p, a) - l0) / (1.0 - l0);
        return (prevK + (p.kMax - prevK) * f) / p.kMax;
    }

    void reset() noexcept
    {
        u.fill (0.0);
        fOld.fill (0.0);
        hpIn = hpOut = 0.0;
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
            fb = FilterOutputStage::transfer (-hp, slope) / stageGain;
            dfb = -slope / stageGain * hpB; // d(out)/d(x3) = slope * (-hpB), over stageGain: +hpB near 0
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
        return p.outputTrim * FilterOutputStage::transfer (-hp, slope);
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

    static constexpr double stageGain = -FilterOutputStage::smallSignalGain; // 68.3: g() has unit slope
    LadderParams p;
    double hpB = 1.0, hpC = 1.0, hpIn = 0.0, hpOut = 0.0;
    Rng rng;
    double fs = 48000.0, h = 1.0 / 48000.0;
    double vt = phys::thermalVoltage (298.15);
    double iTail = 1.0e-5, k = 0.0;
    int maxIter = 8;
    std::array<double, 4> s {}, u {}, fOld {};
};

} // namespace mm
