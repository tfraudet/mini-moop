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
// Oscillator core and waveshapers (CA3046-era oscillator board).
//
// Core: the expo converter sinks current I from a 0.01 uF polystyrene timing
// capacitor; the ramp runs over ~5 V until a comparator with a little
// positive feedback fires and a FET discharges the capacitor back to its
// start level. The reset is not instantaneous: comparator delay + FET
// discharge give a dead time t_reset during which the ramp is held, which
// makes the high octaves go flat (compensated by the HF trimmer).
//
//   period = C * dV / I + t_reset
//
// Waveshapers (from the buffered sawtooth):
//   triangle    : transistor fold of the sawtooth (rounded fold point)
//   sharktooth  : triangle + sawtooth mixed through two resistors (osc 1, 2)
//   reverse saw : inverted sawtooth (osc 3 instead of sharktooth)
//   rectangles  : comparator against the sawtooth at three thresholds
//                 (~50 %, ~30 %, ~15 %), slew-limited output stage
//
// Discontinuities (ramp reset, comparator edges) are located with sub-sample
// precision and corrected with a 2-point polyBLEP residual, so the simulated
// waveform stays exact while aliasing is suppressed.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

enum class Waveform
{
    Triangle = 0,
    SharkTooth,   // reverse sawtooth on oscillator 3
    Sawtooth,
    Square,
    WideRect,
    NarrowRect
};

struct VcoParams
{
    double c          = 0.01e-6;   // timing capacitor, polystyrene 2 %
    double swing      = 5.0;       // ramp amplitude across the capacitor (V)
    double tReset     = 1.6e-6;    // reset dead time (s)
    double outPeak    = 2.5;       // waveshaper output peak (V)
    double foldRound  = 0.03;      // triangle fold rounding (fraction of swing)
    double sharkMix   = 0.5;       // sawtooth weight in the sharktooth mix
    double thrSquare  = 0.0;       // comparator thresholds, normalised ramp
    double thrWide    = -0.4;
    double thrNarrow  = -0.7;
    double pulseSlew  = 2.0e6;     // V/s of the rectangle output stage
    double thrJitter  = 4.0e-5;    // comparator noise (normalised), per cycle

    static VcoParams drawn (Rng& rng, const VcoParams& nominal) noexcept
    {
        VcoParams p = nominal;
        p.c = rng.tolerance (nominal.c, 0.02);
        p.tReset = rng.tolerance (nominal.tReset, 0.2);
        p.thrSquare = nominal.thrSquare + 0.01 * rng.normal();
        p.thrWide = nominal.thrWide + 0.01 * rng.normal();
        p.thrNarrow = nominal.thrNarrow + 0.01 * rng.normal();
        p.sharkMix = rng.tolerance (nominal.sharkMix, 0.05);
        return p;
    }
};

class VcoCore
{
public:
    void prepare (double sampleRate, uint64_t seed) noexcept
    {
        fs = sampleRate;
        h = 1.0 / fs;
        rng.reseed (seed);
        slewStep = p.pulseSlew * h;
        x = 2.0 * rng.uniform() - 1.0; // free-running: random phase at power-up
        hold = 0.0;
        prevNaive = pulseState = 0.0;
        pendingCorrection = 0.0;
    }

    void setParams (const VcoParams& params) noexcept { p = params; slewStep = p.pulseSlew * h; }
    const VcoParams& params() const noexcept { return p; }

    void setWaveform (Waveform w, bool isOsc3) noexcept { wave = w; osc3 = isOsc3; }

    /** Frequency (Hz) for a given expo current, including the reset dead time. */
    double frequencyForCurrent (double amps) const noexcept
    {
        return 1.0 / (p.c * p.swing / std::fmax (amps, 1e-15) + p.tReset);
    }

    /** Advance one sample with the expo converter current `amps`; returns volts. */
    double process (double amps) noexcept
    {
        const double rate = 2.0 * std::fmax (amps, 0.0) / (p.c * p.swing); // normalised ramp units / s
        double t = h;
        double correctionNow = pendingCorrection; // residuals belonging to this sample
        double correctionNext = 0.0;

        // 2-point polyBLEP. An event of height A at tau samples before the end
        // of this sample needs +A*tau^2/2 on the previous sample and
        // -A*(1-tau)^2/2 on this one. Output is delayed by one sample so that
        // the previous sample can still be corrected.
        auto addEvent = [&] (double jump, double tSinceEvent) {
            const double tau = tSinceEvent / h; // 0..1
            correctionNow += jump * 0.5 * tau * tau;
            correctionNext -= jump * 0.5 * (1.0 - tau) * (1.0 - tau);
        };

        int guard = 0;
        while (t > 0.0 && guard++ < 16)
        {
            if (hold > 0.0)
            {
                const double use = std::fmin (t, hold);
                hold -= use;
                t -= use;
                continue;
            }
            if (rate <= 0.0)
                break;

            const double tReset = (1.0 - x) / rate;
            const double thr = pulseThreshold();
            const double tThr = (isPulse() && x < thr) ? (thr - x) / rate : 1e30;

            if (tThr <= t && tThr < tReset)
            {
                x = thr;
                t -= tThr;
                const double before = shape (x - 1e-9), after = shape (x + 1e-9);
                addEvent (after - before, t);
                continue;
            }
            if (tReset <= t)
            {
                const double before = shape (1.0);
                x = -1.0;
                t -= tReset;
                addEvent (shape (-1.0) - before, t);
                hold = p.tReset;
                thrNoise = p.thrJitter * rng.normal();
                continue;
            }
            x += rate * t;
            t = 0.0;
        }

        // Naive value at the end of this sample; the output is one sample late.
        const double naive = shape (x);
        double y = prevNaive + correctionNow;
        prevNaive = naive;
        pendingCorrection = correctionNext;

        if (isPulse())
        {
            // comparator output stage: slew-limited
            const double d = std::fmax (-slewStep, std::fmin (slewStep, y - pulseState));
            pulseState += d;
            y = pulseState;
        }
        return y;
    }

    /** Normalised ramp position (-1..1), for tests and modulation taps. */
    double ramp() const noexcept { return x; }

    /** Waveshaper output for a normalised ramp position (volts). */
    double shape (double r) const noexcept
    {
        const double a = p.outPeak;
        switch (wave)
        {
            case Waveform::Triangle:    return a * triangle (r);
            case Waveform::SharkTooth:
                if (osc3) return -a * r; // reverse sawtooth
                return a * ((1.0 - p.sharkMix) * triangle (r) + p.sharkMix * r);
            case Waveform::Sawtooth:    return a * r;
            case Waveform::Square:
            case Waveform::WideRect:
            case Waveform::NarrowRect:  return r >= pulseThreshold() ? a : -a;
        }
        return 0.0;
    }

private:
    double triangle (double r) const noexcept
    {
        // Fold point rounded by the transistor knee: smooth |r|
        const double e = p.foldRound;
        const double peak = std::sqrt (1.0 + e * e) - e;
        return 2.0 * (std::sqrt (r * r + e * e) - e) / peak - 1.0;
    }

    bool isPulse() const noexcept { return wave == Waveform::Square || wave == Waveform::WideRect || wave == Waveform::NarrowRect; }

    double pulseThreshold() const noexcept
    {
        const double base = wave == Waveform::WideRect ? p.thrWide : wave == Waveform::NarrowRect ? p.thrNarrow : p.thrSquare;
        return base + thrNoise;
    }

    VcoParams p;
    Rng rng;
    Waveform wave = Waveform::Sawtooth;
    bool osc3 = false;
    double fs = 48000.0, h = 1.0 / 48000.0, slewStep = 1.0;
    double x = 0.0, hold = 0.0, thrNoise = 0.0;
    double prevNaive = 0.0, pulseState = 0.0, pendingCorrection = 0.0;
};

} // namespace mm
