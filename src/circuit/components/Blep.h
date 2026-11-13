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
// Band-limited steps and corners (BLEP and BLAMP) at sub-sample times, from
// one windowed sinc: a low-pass at 0.48 of the sample rate, Kaiser-windowed
// (beta 8) over 32 taps, linear phase, tabulated at 256 points a sample. A
// numerical choice, not a circuit value: tests/test_vco.cpp holds it to the
// aliasing it must keep out of the audio band (below -100 dB for the sawtooth
// and the rectangles), which a 2-point polyBLEP does not reach.
//
// BlepLine holds a waveform's naive samples for `delay` samples so that a
// discontinuity can correct the samples on both sides of it: a value jump
// adds the band-limited step's residual (step - its ideal), a slope jump the
// band-limited ramp's. The output comes `delay` samples late.

#include <algorithm>
#include <array>
#include <cmath>

namespace mm
{

struct BlepTables
{
    static constexpr int half = 16;                 // kernel reach each side, samples
    static constexpr int perSample = 256;           // table points per sample
    static constexpr int points = 2 * half * perSample + 1;
    std::array<double, points> step {};             // band-limited step (0 to 1)
    std::array<double, points> ramp {};             // band-limited ramp (its integral)

    BlepTables() noexcept
    {
        auto bessel0 = [] (double x) {
            double s = 1.0, t = 1.0;
            for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
            return s;
        };
        constexpr double cutoff = 0.48, beta = 8.0;
        const double dx = 1.0 / perSample;
        std::array<double, points> kernel {};
        double area = 0.0;
        for (int i = 0; i < points; ++i)
        {
            const double x = -half + i * dx;
            const double a = 2.0 * M_PI * cutoff * x;
            const double sinc = std::abs (a) < 1e-12 ? 1.0 : std::sin (a) / a;
            const double r = x / half;
            const double w = bessel0 (beta * std::sqrt (std::fmax (0.0, 1.0 - r * r))) / bessel0 (beta);
            kernel[size_t (i)] = sinc * w;
            area += kernel[size_t (i)] * dx;
        }
        // Integrate (trapezoidal) into the step, and the step into the ramp.
        double s = 0.0, q = 0.0, prevK = 0.0, prevS = 0.0;
        for (int i = 0; i < points; ++i)
        {
            const double k = kernel[size_t (i)] / area;
            if (i > 0) s += 0.5 * (k + prevK) * dx;
            if (i > 0) q += 0.5 * (s + prevS) * dx;
            prevK = k;
            prevS = s;
            step[size_t (i)] = s;
            ramp[size_t (i)] = q;
        }
        // Beyond the kernel the ramp must be the ideal one: remove the
        // integration's tiny drift so that it ends at `half`.
        const double drift = ramp.back() - half;
        for (int i = 0; i < points; ++i)
            ramp[size_t (i)] -= drift * double (i) / double (points - 1);
    }

    /** Linear interpolation of a (smooth) table at `x` samples from the event. */
    static double lookup (const std::array<double, points>& t, double x) noexcept
    {
        const double u = (x + half) * perSample;
        const int i = std::min (int (u), points - 2);
        const double f = u - double (i);
        return t[size_t (i)] + f * (t[size_t (i + 1)] - t[size_t (i)]);
    }

    /** Band-limited step minus the ideal step, `x` samples from the event. The
        ideal is subtracted after interpolating the smooth curve, so that the
        jump at 0 is exact wherever the sample falls. */
    double stepResidual (double x) const noexcept
    {
        if (x <= -half || x >= half) return 0.0;
        return lookup (step, x) - (x >= 0.0 ? 1.0 : 0.0);
    }

    /** Band-limited ramp minus the ideal ramp. */
    double rampResidual (double x) const noexcept
    {
        if (x <= -half || x >= half) return 0.0;
        return lookup (ramp, x) - (x > 0.0 ? x : 0.0);
    }

    /** The process's tables (made on first use: call it outside the audio thread). */
    static const BlepTables& get() noexcept
    {
        static const BlepTables tables;
        return tables;
    }
};

class BlepLine
{
public:
    static constexpr int delay = BlepTables::half;

    void reset (double value = 0.0) noexcept
    {
        buf.fill (0.0);
        for (int j = 1; j <= delay; ++j) buf[size_t ((w - j) & mask)] = value; // the samples still to come out
    }

    /** A value jump of `height`, `after` samples before the current sample
        (0..1: the event lies between the previous sample and this one). */
    void step (double height, double after) noexcept
    {
        const auto& t = BlepTables::get();
        for (int j = -delay; j <= delay; ++j)
            buf[size_t ((w + j) & mask)] += height * t.stepResidual (j + after);
    }

    /** A slope jump of `slope` (value per sample), `after` samples before the
        current sample. */
    void corner (double slope, double after) noexcept
    {
        const auto& t = BlepTables::get();
        for (int j = -delay; j <= delay; ++j)
            buf[size_t ((w + j) & mask)] += slope * t.rampResidual (j + after);
    }

    /** Adds the current sample's naive value; returns the band-limited sample
        `delay` samples back, and moves on. Corrections of the current sample
        (step() / corner()) must come before. */
    double push (double naive) noexcept
    {
        buf[size_t (w & mask)] += naive;
        const size_t out = size_t ((w - delay) & mask);
        const double y = buf[out];
        buf[out] = 0.0;
        w = (w + 1) & mask;
        return y;
    }

private:
    static constexpr int size = 64, mask = size - 1;
    std::array<double, size> buf {};
    int w = 0;
};

} // namespace mm
