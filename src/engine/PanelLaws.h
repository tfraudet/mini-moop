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
// Panel knob laws shared by the engine and the parameter display.

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace mm::laws
{

/** Interpolates (x, y) points, straight between them, clamped at the ends. */
template <std::size_t N>
double piecewise (const std::array<std::pair<double, double>, N>& pts, double x)
{
    if (x <= pts.front().first) return pts.front().second;
    for (std::size_t i = 1; i < N; ++i)
        if (x <= pts[i].first)
            return pts[i - 1].second + (pts[i].second - pts[i - 1].second) * (x - pts[i - 1].first) / (pts[i].first - pts[i - 1].first);
    return pts.back().second;
}

/** CUTOFF FREQUENCY's law, from Moog's service manual. Filter Range (Table
    5-4): KEYBOARD CONTROL 1 and 2 off, CUTOFF at -1, EMPHASIS at 10, the
    filter is trimmed to zero beats with the A-440, so it sings at 440 Hz at
    the -1 mark. Specifications: "Continuously variable from 40Hz to 20kHz
    (9 octaves)": 9 octaves over the dial's -5..+5, a linear pot into the
    1 V/octave converter, 0.9 octave a mark (5.18: "approximately 2
    divisions" for two octaves; 5.13: at least 16 kHz fully clockwise, under
    300 Hz fully anticlockwise). `pos` is the knob 0..1 (-5..+5). */
inline constexpr double cutoffCalMark = -1.0;
inline constexpr double cutoffCalHz = 440.0;
inline constexpr double cutoffOctavesPerMark = 0.9;

/** Octaves above the calibration point (-1 mark). */
inline double cutoffOctaves (double pos)
{
    return cutoffOctavesPerMark * ((pos - 0.5) * 10.0 - cutoffCalMark);
}

/** The frequency the filter sings at for a CUTOFF knob position 0..1
    (KEYBOARD CONTROL off, no contour, no modulation): 36 Hz to 18.5 kHz. */
inline double cutoffHz (double pos)
{
    return cutoffCalHz * std::exp2 (cutoffOctaves (pos));
}

} // namespace mm::laws
