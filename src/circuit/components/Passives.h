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
// Passive component helpers: deterministic tolerance draws and potentiometer tapers.

#include <cmath>
#include <cstdint>

namespace mm
{

/** Small deterministic PRNG (xoshiro256**), so a given "board seed" always
    produces the same set of component values. Real-time safe. */
class Rng
{
public:
    explicit Rng (uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept { reseed (seed); }

    void reseed (uint64_t seed) noexcept
    {
        uint64_t z = seed;
        for (auto& v : s)
        {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            v = x ^ (x >> 31);
        }
        haveSpare = false;
    }

    uint64_t next() noexcept
    {
        const uint64_t result = rotl (s[1] * 5, 7) * 9;
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t; s[3] = rotl (s[3], 45);
        return result;
    }

    /** Uniform in [0, 1). */
    double uniform() noexcept { return double (next() >> 11) * 0x1.0p-53; }

    /** Uniform in [-1, 1). */
    double bipolar() noexcept { return 2.0 * uniform() - 1.0; }

    /** Standard normal (Marsaglia polar). */
    double normal() noexcept
    {
        if (haveSpare) { haveSpare = false; return spare; }
        double u, v, q;
        do { u = bipolar(); v = bipolar(); q = u * u + v * v; } while (q >= 1.0 || q == 0.0);
        const double m = std::sqrt (-2.0 * std::log (q) / q);
        spare = v * m; haveSpare = true;
        return u * m;
    }

    /** Component value with a manufacturing tolerance. `tol` is the rated
        tolerance (e.g. 0.05 for 5 %); parts are modelled as a normal
        distribution with the rated value at 2 sigma, clipped to the rating. */
    double tolerance (double nominal, double tol) noexcept
    {
        double d = 0.5 * tol * normal();
        d = std::fmax (-tol, std::fmin (tol, d));
        return nominal * (1.0 + d);
    }

    /** The same for an asymmetric rating, -`below` to +`above` (aluminium
        electrolytics: "+50/-10%"): normal about the middle of the range, its
        ends at 2 sigma, clipped to them. */
    double tolerance (double nominal, double below, double above) noexcept
    {
        const double mid = 0.5 * (above - below), half = 0.5 * (above + below);
        double d = mid + 0.5 * half * normal();
        d = std::fmax (-below, std::fmin (above, d));
        return nominal * (1.0 + d);
    }

private:
    static uint64_t rotl (uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }
    uint64_t s[4] {};
    double spare = 0.0;
    bool haveSpare = false;
};

/** Potentiometer tapers. Position p in [0, 1] -> fraction of track resistance. */
namespace taper
{
inline double linear (double p) noexcept { return p; }

/** "Audio" (logarithmic) taper: resistance from the counter-clockwise end,
    (b^p - 1) / (b - 1), with `midFraction` of the track at half rotation
    (b = ((1 - m) / m)^2; 10 % gives b = 81). The drawings only name the
    tapers ("AUDIO", "REV. AUDIO", "No. 1 TAPER"); 10 % at half rotation is the
    usual figure for the period's carbon pots, and it puts EMPHASIS's
    oscillation threshold within REGEN CAL's range (LadderFilter.h). A reverse
    audio pot is 1 - audio (1 - p) from the same end. */
inline double audio (double p, double midFraction = 0.10) noexcept
{
    const double x = p < 0.0 ? 0.0 : p > 1.0 ? 1.0 : p;
    const double r = (1.0 - midFraction) / midFraction;
    const double b = r * r;
    return (std::pow (b, x) - 1.0) / (b - 1.0);
}

/** Smooth exponential approximation of a log taper (for time constants). */
inline double expo (double p, double ratio) noexcept
{
    // ratio = R(max)/R(min) of the usable range
    return (std::pow (ratio, p) - 1.0) / (ratio - 1.0);
}
} // namespace taper

/** Output of a loaded potentiometer used as a voltage divider:
    wiper fraction `a` of track R, load resistance `rl` from wiper to ground. */
inline double loadedPotDivider (double a, double r, double rl) noexcept
{
    const double rTop = (1.0 - a) * r;
    const double rBot = a * r;
    const double rBotL = rBot * rl / (rBot + rl + 1e-30);
    return rBotL / (rTop + rBotL + 1e-30);
}

} // namespace mm
