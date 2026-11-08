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
// Cascaded half-band FIR decimators (factor 1, 2, 4 or 8). The circuit
// simulation runs at the oversampled rate; this brings it back to the host
// rate with > 90 dB rejection of the folded band.

#include <array>
#include <cmath>
#include <vector>

namespace mm
{

class HalfbandDecimator
{
public:
    /** `numTaps` must be of the form 4k+3. Kaiser-windowed sinc. */
    void design (int numTaps, double kaiserBeta)
    {
        taps = numTaps;
        const int c = numTaps / 2;
        coeffs.assign (size_t (numTaps), 0.0);
        auto bessel0 = [] (double x) {
            double s = 1.0, t = 1.0;
            for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
            return s;
        };
        double sum = 0.0;
        for (int n = 0; n < numTaps; ++n)
        {
            const int m = n - c;
            const double sinc = m == 0 ? 0.5 : std::sin (M_PI * m / 2.0) / (M_PI * m);
            const double r = double (m) / double (c);
            const double w = bessel0 (kaiserBeta * std::sqrt (std::fmax (0.0, 1.0 - r * r))) / bessel0 (kaiserBeta);
            coeffs[size_t (n)] = (m != 0 && m % 2 == 0) ? 0.0 : sinc * w;
            sum += coeffs[size_t (n)];
        }
        for (auto& v : coeffs) v /= sum;
        buffer.assign (size_t (2 * numTaps), 0.0);
        pos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0); pos = 0; }

    /** Push two input samples, get one output sample. */
    double process (double a, double b) noexcept
    {
        push (a);
        push (b);
        // Convolution over the circular buffer; zero taps are skipped.
        const double* base = buffer.data() + pos; // pos..pos+taps-1 is the latest window (mirrored)
        double acc = coeffs[size_t (taps / 2)] * base[taps / 2];
        for (int n = 0; n < taps / 2; n += 2)
            acc += coeffs[size_t (n)] * (base[n] + base[taps - 1 - n]);
        return acc;
    }

private:
    void push (double x) noexcept
    {
        // Mirrored circular buffer so that the window is always contiguous.
        pos = (pos == 0 ? taps : pos) - 1;
        buffer[size_t (pos)] = x;
        buffer[size_t (pos + taps)] = x;
    }

    std::vector<double> coeffs, buffer;
    int taps = 0, pos = 0;
};

class Decimator
{
public:
    /** factor = 1, 2, 4 or 8 */
    void prepare (int factor)
    {
        stages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
        // Last stage (closest to the host rate) does the steep work.
        for (int s = 0; s < stages; ++s)
        {
            const bool last = (s == stages - 1);
            hb[size_t (s)].design (last ? 79 : 31, last ? 9.0 : 8.0);
        }
        work.assign (8, 0.0);
    }

    int factor() const noexcept { return 1 << stages; }

    void reset() { for (auto& h : hb) h.reset(); }

    /** Consumes factor() input samples, returns one output sample. */
    double process (const double* in) noexcept
    {
        if (stages == 0) return in[0];
        int n = 1 << stages;
        for (int i = 0; i < n; ++i) work[size_t (i)] = in[i];
        for (int s = 0; s < stages; ++s)
        {
            n /= 2;
            for (int i = 0; i < n; ++i)
                work[size_t (i)] = hb[size_t (s)].process (work[size_t (2 * i)], work[size_t (2 * i + 1)]);
        }
        return work[0];
    }

private:
    std::array<HalfbandDecimator, 3> hb;
    std::vector<double> work;
    int stages = 0;
};

} // namespace mm
