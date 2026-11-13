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
//
// The amplifier is discrete, and only one way strong: Q7 (2N4058) sources
// current up from +10 V, while the output's only pull-down is R30 10K to
// -10 V. Below zero the output stops where R30 meets what it drives: R57 1K
// and the line (the MODULATION wheel, a rheostat to ground, and the loads the
// switches connect), and R60 back to the summing node. With the wheel fully
// forward that is -1.7 V, near its back end -1 V: oscillator 3's sawtooth
// and rectangles, and the noise's peaks, are cut on that side. ngspice on the
// drawn circuit (tools/spice/modulation.cir): a gain of 0.829 where the
// virtual ground gives 0.840, then a floor within 0.01 V of the divider's.

#include <cmath>
#include <utility>

namespace mm
{

struct ModulationMix
{
    static constexpr double rIn = 24.0e3;       // R23 / R24: series resistor of each source
    static constexpr double rPot = 25.0e3;      // R3, linear
    static constexpr double rSum = 43.0e3;      // R62, R64
    static constexpr double rFeedback = 91.0e3; // R60: the amplifier's feedback
    static constexpr double rOut = 1.0e3;       // R57: from the output to the line
    static constexpr double rPullDown = 10.0e3; // R30: the output's pull-down to -10 V
    static constexpr double loopLoss = 0.987;   // the discrete amplifier's finite gain: 0.829 for 0.840 (ngspice)
    /** The output's standing offset (V): Q18's base current through R60, Q17's
        base being grounded directly (ngspice, a 2N3392 at a gain of 210). It
        reaches the line through the wheel's divider: a few cents of pitch
        with the wheel forward and OSC. MODULATION on. */
    static constexpr double outputOffset = 0.041;
    /** What else the output drives when Q7 is off: R60 into the summing node
        and, behind it, the inputs' networks (about 120K; ngspice's floors). */
    static constexpr double rFeedbackSide = 120.0e3;

    /** The output's lowest voltage, Q7 off, for the line's resistance to ground. */
    static double outputFloor (double rLine) noexcept
    {
        const double rl = 1.0 / (1.0 / (rOut + rLine) + 1.0 / rFeedbackSide);
        return -10.0 * rl / (rPullDown + rl);
    }

    /** The output (V) for its linear value `v`: as Q7's current falls the
        loop loses gain, and the output bends into the floor along a hyperbola
        0.11 V wide (ngspice: 6 mV short of the line 0.5 V above the floor,
        20 mV 0.12 V above it). */
    static double limit (double v, double floor) noexcept
    {
        constexpr double knee = 0.11;
        const double d = v - floor;
        return floor + 0.5 * (d + std::sqrt (d * d + knee * knee));
    }

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
        return gIn / (gIn + gW + gSum) * (rFeedback / rSum) * loopLoss;
    }
};

} // namespace mm
