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
// Core (service manual 2.16): the expo converter sinks current I from the
// 0.01 uF polystyrene timing capacitor C1; the ramp runs from 0 to -4 V, where
// the Schmitt trigger Q5 / Q6 fires and Q10 discharges the capacitor. A x2 FET
// amplifier turns it into the +4 to -4 V sawtooth on Q3's collector. The reset is not instantaneous: comparator delay + FET
// discharge give a dead time t_reset during which the ramp is held, which
// makes the high octaves go flat (compensated by the HF trimmer).
//
//   period = C * dV / I + t_reset
//
// Waveshapers (from the buffered sawtooth), each with the level Fig. 9-3 gives:
//   sawtooth    : Q3's collector through R34 4300 / R33 4700: +/-2.09 V
//   triangle    : transistor fold of the sawtooth by Q2 (rounded fold point),
//                 follower Q1 and R28 / R23: "+1.7 TO -1.7V"
//   sharktooth  : sawtooth and triangle mixed through the front panel's R030
//                 47K and R031 10K (drawing 1448, Table 7-2), against the
//                 board's output dividers (osc 1, 2): 18 % sawtooth
//   reverse saw : sawtooth inverted by Q37 (osc 3 instead of sharktooth):
//                 "+1.7 TO -1.7V LOADED"
//   rectangles  : Schmitt trigger Q11 / Q12 on the sawtooth (R1 43K) summed with
//                 a bias (R40 43K) that the WAVEFORM switch takes from -10 V,
//                 R32 7.5K, R33 1K, R34 1.5K, ground (Fig. 9-16): 0, -1.5 and
//                 -2.5 V, widths of 50, 31 and 19 % ("from a 50 percent to a 15
//                 percent duty cycle", 2.16); "(3.5V)" peak to peak
//
// Discontinuities are located with sub-sample precision and band-limited
// with a windowed-sinc kernel (BlepLine): value jumps (the ramp's reset, the
// comparator's edges) with band-limited steps, slope jumps (the reset, the end
// of the reset's hold, the triangle's fold) with band-limited ramps. The
// output comes BlepLine::delay samples late.

#include "../components/Blep.h"
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
    double swing      = 4.0;       // ramp amplitude across the capacitor: 0 to -4 V (Fig. 9-3)
    double tReset     = 1.6e-6;    // reset dead time (s)
    double sawPeak    = 4.0 * 4.7e3 / 9.0e3; // sawtooth: +/-4 V through R34 4300 / R33 4700 (Fig. 9-3), unloaded
    double triPeak    = 1.747;     // triangle: "+1.7 TO -1.7V" (Fig. 9-3); ngspice on the drawn shaper (shapers.cir):
                                   // 3.49 V peak to peak about -0.21 V
    double rectPeak   = 1.75;      // rectangle: "(3.5V)" peak to peak (Fig. 9-3)
    double revSawPeak = 1.7;       // oscillator 3's reverse sawtooth: "+1.7 TO -1.7V LOADED" (Fig. 9-3)
    double foldRound  = 0.021;     // triangle fold rounding (fraction of swing): Q2's knee, fitted to shapers.cir
                                   // (11 mV rms from ngspice's transfer)
    double rSawOut    = 4.3e3 * 4.7e3 / 9.0e3; // sawtooth output divider R34 / R33 (Fig. 9-3): its source resistance
    double rTriOut    = 750.0 * 8.2e3 / 8.95e3; // triangle output, R28 / R23 behind Q1 (Fig. 9-3)
    double rSharkSaw  = 47.0e3;    // R030: sawtooth into the shark tooth (drawing 1448, Table 7-2, 5 %)
    double rSharkTri  = 10.0e3;    // R031: triangle into the shark tooth
    // Comparator thresholds, normalised ramp: the Schmitt trigger Q11 / Q12 on
    // the sawtooth and the WAVEFORM switch's bias (0, -1.5, -2.5 V) through
    // equal 43K; ngspice on the drawn circuit (shapers.cir): 50.1, 31.7 and
    // 19.9 % of the cycle.
    double thrSquare  = 0.002;
    double thrWide    = -0.366;
    double thrNarrow  = -0.602;
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
        p.rSharkSaw = rng.tolerance (nominal.rSharkSaw, 0.05);
        p.rSharkTri = rng.tolerance (nominal.rSharkTri, 0.05);
        return p;
    }

    /** The shark tooth's sawtooth weight: each source through its output
        resistance and its front-panel resistor (Millman). */
    double sharkMix() const noexcept
    {
        const double gs = 1.0 / (rSawOut + rSharkSaw), gt = 1.0 / (rTriOut + rSharkTri);
        return gs / (gs + gt);
    }

    /** The shark tooth's source resistance (the two paths in parallel). */
    double rSharkOut() const noexcept
    {
        return 1.0 / (1.0 / (rSawOut + rSharkSaw) + 1.0 / (rTriOut + rSharkTri));
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
        pulseState = 0.0;
        BlepTables::get(); // made here, not on the audio thread
        line.reset (shape (x));
    }

    /** Output delay of the band-limiting, samples. */
    static constexpr int latency = BlepLine::delay;

    void setParams (const VcoParams& params) noexcept { p = params; slewStep = p.pulseSlew * h; }
    const VcoParams& params() const noexcept { return p; }

    /** The WAVEFORM switch. A change restarts the band-limiting line from the
        new shape (the samples still in it belong to the old one). */
    void setWaveform (Waveform w, bool isOsc3) noexcept
    {
        if (w == wave && isOsc3 == osc3)
            return;
        wave = w;
        osc3 = isOsc3;
        line.reset (shape (x));
    }

    /** Frequency (Hz) for a given expo current, including the reset dead time. */
    double frequencyForCurrent (double amps) const noexcept
    {
        return 1.0 / (p.c * p.swing / std::fmax (amps, 1e-15) + p.tReset);
    }

    /** Advance one sample with the expo converter current `amps`; returns volts. */
    double process (double amps) noexcept
    {
        const double rate = 2.0 * std::fmax (amps, 0.0) / (p.c * p.swing); // normalised ramp units / s
        const double perSample = rate * h;                                    // ramp units / sample
        double t = h;

        int guard = 0;
        while (t > 0.0 && guard++ < 16)
        {
            if (hold > 0.0)
            {
                const double use = std::fmin (t, hold);
                hold -= use;
                t -= use;
                if (hold <= 0.0 && rate > 0.0)
                    line.corner (slope (x) * perSample, t / h); // the ramp starts again
                continue;
            }
            if (rate <= 0.0)
                break;

            const double tReset = (1.0 - x) / rate;
            const double thr = pulseThreshold();
            const double tThr = (isPulse() && x < thr) ? (thr - x) / rate : 1e30;
            const double tFold = (hasFold() && x < 0.0) ? -x / rate : 1e30;
            const double tNext = std::fmin (tReset, std::fmin (tThr, tFold));
            if (tNext > t)
            {
                x += rate * t;
                t = 0.0;
                break;
            }
            x += rate * tNext;
            t -= tNext;
            if (tNext == tThr)
            {
                // comparator edge: a value jump
                x = thr;
                line.step (shape (x + 1e-9) - shape (x - 1e-9), t / h);
            }
            else if (tNext == tFold)
            {
                // the triangle's fold: a rounded corner (the transistors'
                // knee), as eight band-limited ramps spread where its slope
                // turns (equal slope steps). Wider than a sample, the
                // sampled curve is already smooth and it is left as it is.
                x = 0.0;
                constexpr double pieces[] = { 0.126, 0.405, 0.801, 1.81 }; // r / foldRound at slope 1/8, 3/8, 5/8, 7/8
                const double reach = pieces[3] * p.foldRound / std::fmax (perSample, 1e-30); // samples
                if (reach <= 1.0)
                {
                    const double each = (foldSlope (+1) - foldSlope (-1)) * perSample / 8.0;
                    for (double q : pieces)
                    {
                        const double shift = q * p.foldRound / perSample;
                        line.corner (each, t / h + shift); // before the crossing
                        line.corner (each, t / h - shift); // after it
                    }
                }
                x = 1e-12; // past the fold
            }
            else
            {
                // reset: the ramp jumps back and holds for the dead time
                const double before = shape (1.0), slopeBefore = slope (1.0);
                x = -1.0;
                line.step (shape (-1.0) - before, t / h);
                line.corner (-slopeBefore * perSample, t / h);
                hold = p.tReset;
                thrNoise = p.thrJitter * rng.normal();
                if (hold <= 0.0)
                    line.corner (slope (-1.0) * perSample, t / h);
            }
        }

        double y = line.push (shape (x));

        if (isPulse())
        {
            // comparator output stage: slew-limited
            const double d = std::fmax (-slewStep, std::fmin (slewStep, y - pulseState));
            pulseState += d;
            y = pulseState;
        }
        return y;
    }

    /** The selected output's source resistance (ohm), loading the mixer's
        VOLUME pot. Rectangles and oscillator 3's reverse sawtooth: not
        modelled (0). */
    double sourceResistance() const noexcept
    {
        switch (wave)
        {
            case Waveform::Triangle:   return p.rTriOut;
            case Waveform::Sawtooth:   return p.rSawOut;
            case Waveform::SharkTooth: return osc3 ? 0.0 : p.rSharkOut();
            default:                   return 0.0;
        }
    }

    /** Normalised ramp position (-1..1), for tests and modulation taps. */
    double ramp() const noexcept { return x; }

    /** Waveshaper output for a normalised ramp position (volts). */
    double shape (double r) const noexcept
    {
        switch (wave)
        {
            case Waveform::Triangle:    return p.triPeak * triangle (r);
            case Waveform::SharkTooth:
                if (osc3) return -p.revSawPeak * r; // reverse sawtooth
                return (1.0 - p.sharkMix()) * p.triPeak * triangle (r) + p.sharkMix() * p.sawPeak * r;
            case Waveform::Sawtooth:    return p.sawPeak * r;
            case Waveform::Square:
            case Waveform::WideRect:
            case Waveform::NarrowRect:  return r >= pulseThreshold() ? p.rectPeak : -p.rectPeak;
        }
        return 0.0;
    }

private:
    /** d(shape)/d(ramp) at `r`, volts per ramp unit. */
    double slope (double r) const noexcept
    {
        switch (wave)
        {
            case Waveform::Triangle:    return p.triPeak * triangleSlope (r);
            case Waveform::SharkTooth:
                if (osc3) return -p.revSawPeak;
                return (1.0 - p.sharkMix()) * p.triPeak * triangleSlope (r) + p.sharkMix() * p.sawPeak;
            case Waveform::Sawtooth:    return p.sawPeak;
            case Waveform::Square:
            case Waveform::WideRect:
            case Waveform::NarrowRect:  return 0.0;
        }
        return 0.0;
    }

    /** The slope on either side of the fold, as if it were sharp (side -1 / +1). */
    double foldSlope (int side) const noexcept
    {
        const double e = p.foldRound;
        const double peak = std::sqrt (1.0 + e * e) - e;
        const double tri = 2.0 * side / peak;
        return wave == Waveform::Triangle ? p.triPeak * tri
                                          : (1.0 - p.sharkMix()) * p.triPeak * tri + p.sharkMix() * p.sawPeak;
    }

    bool hasFold() const noexcept { return wave == Waveform::Triangle || (wave == Waveform::SharkTooth && ! osc3); }

    double triangleSlope (double r) const noexcept
    {
        const double e = p.foldRound;
        const double peak = std::sqrt (1.0 + e * e) - e;
        return 2.0 * r / std::sqrt (r * r + e * e) / peak;
    }

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
    double pulseState = 0.0;
    BlepLine line;
};

} // namespace mm
