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

/** CUTOFF FREQUENCY's law. The pot R11 (5K linear) sits between +10 V and
    -10 V and reaches the filter's control node through R55 200K, where 100K is
    an octave a volt (Fig. 9-16): 10 octaves over the dial's -5..+5, an octave
    a mark ("a rotation of approximately 2 divisions" for two octaves, service
    manual 5.18). Filter Range (Folkman; 5.13): KEYBOARD CONTROL 1 and 2 off,
    CUTOFF at -1, EMPHASIS at 10, the filter is trimmed to zero beats with the
    A-440, so it sings at 440 Hz at the -1 mark; "frequency should increase to
    at least 16kHz" fully clockwise. `pos` is the knob 0..1 (-5..+5). */
inline constexpr double cutoffCalMark = -1.0;
inline constexpr double cutoffCalHz = 440.0;
inline constexpr double cutoffOctavesPerMark = 1.0;

/** Knob position 0..1 of a dial mark -5..+5. */
inline constexpr double cutoffKnobAt (double mark) { return 0.5 + mark / 10.0; }

/** Octaves above the calibration point (-1 mark). */
inline double cutoffOctaves (double pos)
{
    return cutoffOctavesPerMark * ((pos - 0.5) * 10.0 - cutoffCalMark);
}

/** The frequency the filter sings at for a CUTOFF knob position 0..1
    (KEYBOARD CONTROL off, no contour, no modulation): 27.5 Hz to 28 kHz. */
inline double cutoffHz (double pos)
{
    return cutoffCalHz * std::exp2 (cutoffOctaves (pos));
}

} // namespace mm::laws
