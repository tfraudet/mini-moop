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
// Main output VOLUME: a passive divider, not a VCA (front panel wiring
// Fig. 9-16, board 4 circuit 7). The output VCA drives board 4 pin 21 through
// R75 1k into the top of R20, a 5k LINEAR pot (Table 7-3) whose other end is
// grounded; the wiper goes through the MAIN OUTPUT ON switch SW19 to the jack.
// The load on the wiper (the input of whatever is plugged in) is assumed to be
// 10k. The gain is the divider's: volts at the jack for a volt at the
// output stage (after C2), 0.77 fully clockwise into that load.

namespace mm
{

struct OutputVolume
{
    static constexpr double rSource = 1.0e3;  // R75
    static constexpr double rPot = 5.0e3;     // R20, linear
    static constexpr double rLoad = 10.0e3;   // assumed input of the next device

    /** Gain for a knob position 0..1: jack volts per volt at R75's input. */
    static double gain (double pos) noexcept
    {
        auto divider = [] (double a) {
            const double bottom = a * rPot * rLoad / (a * rPot + rLoad); // lower track || load
            return bottom / (rSource + (1.0 - a) * rPot + bottom);
        };
        const double a = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
        return divider (a);
    }
};

} // namespace mm
