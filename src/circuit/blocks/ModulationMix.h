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
// grounded, through a 24k resistor. Each end then feeds the summing input of
// the modulation amplifier (Q18) through 43k (R62, R64); R60 91k returns that
// node to the -10 V supply (an AC ground). At 0 the wiper shorts the noise
// end (oscillator 3 only), at 10 it shorts the oscillator 3 end (noise only);
// in between both sources are attenuated by the part of the track that
// remains to ground, which is not a linear crossfade.

#include <utility>

namespace mm
{

struct ModulationMix
{
    static constexpr double rIn = 24.0e3;   // series resistor of each source
    static constexpr double rPot = 25.0e3;  // R3, linear
    static constexpr double rSum = 43.0e3;  // R62, R64
    static constexpr double rBias = 91.0e3; // R60

    /** Weights of oscillator 3 and of the noise at the modulation amplifier for
        a knob position 0..1, each normalised to 1 at its own end of travel. */
    static std::pair<double, double> gains (double pos) noexcept
    {
        const double a = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
        return { raw (a).first / raw (0.0).first, raw (a).second / raw (1.0).second };
    }

private:
    /** Node analysis: A = noise end, B = oscillator 3 end, S = summing node.
        Returns (S / V_osc3, S / V_noise). */
    static std::pair<double, double> raw (double a) noexcept
    {
        constexpr double minR = 1.0; // wiper contact at the very end of the track
        const double gIn = 1.0 / rIn, gSum = 1.0 / rSum, gBias = 1.0 / rBias;
        const double gA = 1.0 / (a * rPot > minR ? a * rPot : minR);                  // noise end to wiper
        const double gB = 1.0 / ((1.0 - a) * rPot > minR ? (1.0 - a) * rPot : minR);  // osc 3 end to wiper
        const double dA = gIn + gA + gSum, dB = gIn + gB + gSum;
        const double k = 2.0 * gSum + gBias - gSum * gSum / dA - gSum * gSum / dB;
        return { gSum * gIn / (dB * k), gSum * gIn / (dA * k) };
    }
};

} // namespace mm
