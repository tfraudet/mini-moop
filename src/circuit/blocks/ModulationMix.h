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
// MODULATION MIX (board 3, circuit 15, Fig. 9-8). Oscillator 3 and the noise
// (pink or red) each feed one end of R3, a 25k LINEAR pot whose wiper is
// grounded, through a 24k resistor. Each end then feeds the summing node of
// the modulation amplifier through 43k (R62, R64). The amplifier (Q18 / Q17,
// Q7) is an inverter: R60 91k from its output back to the summing node holds
// that node at a virtual ground, so the two ends do not load each other and
// the gain from an end is R60 / 43k (about 2, service manual 2.4). At 0 the
// wiper shorts the noise end (oscillator 3 only), at 10 it shorts the
// oscillator 3 end (noise only); in between both sources are attenuated by
// the part of the track that remains to ground, which is not a linear
// crossfade.

#include <utility>

namespace mm
{

struct ModulationMix
{
    static constexpr double rIn = 24.0e3;       // R23 / R24: series resistor of each source
    static constexpr double rPot = 25.0e3;      // R3, linear
    static constexpr double rSum = 43.0e3;      // R62, R64
    static constexpr double rFeedback = 91.0e3; // R60: the amplifier's feedback

    /** The amplifier's output (V) per volt of oscillator 3 and per volt of
        noise, for a knob position 0..1, as magnitudes (the amplifier inverts).
        `rOsc3Source`: oscillator 3's output resistance. */
    static std::pair<double, double> amplifier (double pos, double rOsc3Source = 0.0) noexcept
    {
        const double a = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
        return { end ((1.0 - a) * rPot, rOsc3Source), end (a * rPot, 0.0) };
    }

    /** Weights of oscillator 3 and of the noise, each normalised to 1 at its
        own end of travel. */
    static std::pair<double, double> gains (double pos) noexcept
    {
        const auto g = amplifier (pos);
        return { g.first / amplifier (0.0).first, g.second / amplifier (1.0).second };
    }

private:
    /** One end: the source through rIn into the node, loaded by the track to
        the wiper (ground) and by the 43k into the virtual ground; that 43k's
        current is the amplifier's input, R60 its gain. */
    static double end (double rToWiper, double rSource) noexcept
    {
        constexpr double minR = 1.0; // wiper contact at the very end of the track
        const double gIn = 1.0 / (rIn + rSource), gW = 1.0 / (rToWiper > minR ? rToWiper : minR), gSum = 1.0 / rSum;
        return gIn / (gIn + gW + gSum) * (rFeedback / rSum);
    }
};

} // namespace mm
