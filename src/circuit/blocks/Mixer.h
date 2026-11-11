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
// Five-channel mixer: each source drives a volume pot (with ON/OFF switch);
// the wipers are summed through resistors into the filter input node.
//
// The pots are loaded by their summing resistors (the pot law is therefore
// not the bare taper), and there is no active gain stage before the filter:
// turning several channels up drives the filter's input differential pair
// into saturation, which is the characteristic "mixer overdrive" of the
// instrument. The model returns the summed node voltage; the saturation
// itself happens in the LadderFilter's input pair (tanh).
//
// Values from the front panel assembly (service manual Fig. 9-16, Tables 7-2
// and 7-3): 25 kohm linear volume pots (R6 osc 1, R7 osc 2, R8 osc 3, R10
// noise) and summing resistors on the ON/OFF switches (R45, R46, R47, R49 =
// 33 kohm, R48 = 11 kohm for noise). The external input volume R9 (1 Mohm
// audio) sits ahead of the board 4 pre-amplifier (ExtPreamp), so the ext
// channel is driven by the pre-amp output: set its pot to 1, only its ON
// switch applies here.

#include "../components/Passives.h"

#include <array>

namespace mm
{

struct MixerParams
{
    static constexpr int numChannels = 5; // osc1, ext, osc2, noise, osc3

    // Channel order: osc 1, ext, osc 2, noise, osc 3
    std::array<double, numChannels> rPot { 25e3, 0.0, 25e3, 25e3, 25e3 };     // volume pots (ext: pre-amp output)
    std::array<bool, numChannels> linearPot { true, false, true, true, true }; // ext R9 is audio taper
    std::array<double, numChannels> rSum { 33e3, 33e3, 33e3, 11e3, 33e3 };    // R45, R46, R47, R48, R49
    double rNode = 470.0;                   // R54: the node feeds the base of Q29 (bottom pair) through
                                            // C27 10 uF, returned by R54 to the AC-grounded bias (Fig. 9-11)
    double nodeGain = 1.0;                  // no buffer: the node voltage is the pair input

    static MixerParams drawn (Rng& rng, const MixerParams& nominal) noexcept
    {
        MixerParams p = nominal;
        for (auto& r : p.rSum) r = rng.tolerance (r, 0.05);
        p.rNode = rng.tolerance (nominal.rNode, 0.05);
        for (auto& r : p.rPot) r = rng.tolerance (r, 0.20); // carbon pots: 20 %
        return p;
    }
};

class Mixer
{
public:
    void setParams (const MixerParams& params) noexcept { p = params; update(); }

    /** Volume pot position 0..1 and switch per channel. */
    void setChannel (int ch, double pos, bool on) noexcept
    {
        pots[size_t (ch)] = pos;
        switches[size_t (ch)] = on;
        update();
    }

    /** Source resistance of a channel's input (ohm): the output it comes
        from, in series with the top of the VOLUME pot. */
    void setSourceResistance (int ch, double ohms) noexcept
    {
        if (rSource[size_t (ch)] == ohms) return;
        rSource[size_t (ch)] = ohms;
        update();
    }

    double process (const std::array<double, MixerParams::numChannels>& in) const noexcept
    {
        double v = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
            v += gains[i] * in[i];
        return v;
    }

private:
    void update() noexcept
    {
        // Node voltage by superposition (Millman): sum of wiper voltages through
        // their summing resistors into the node resistor. Wiper source
        // resistance is that of the loaded pot.
        double gTotal = 1.0 / p.rNode;
        std::array<double, MixerParams::numChannels> rSrc {}, frac {};
        for (size_t i = 0; i < rSrc.size(); ++i)
        {
            frac[i] = p.linearPot[i] ? taper::linear (pots[i]) : taper::audio (pots[i]);
            // Thevenin at the wiper of the pot driven through the source resistance
            const double lower = frac[i] * p.rPot[i], upper = (1.0 - frac[i]) * p.rPot[i] + rSource[i];
            const double rWiper = p.rPot[i] > 0.0 ? lower * upper / (lower + upper) : 0.0;
            attenuation[i] = p.rPot[i] > 0.0 ? p.rPot[i] / (p.rPot[i] + rSource[i]) : 1.0;
            rSrc[i] = p.rSum[i] + rWiper;
            gTotal += 1.0 / rSrc[i];
        }
        for (size_t i = 0; i < rSrc.size(); ++i)
        {
            const double a = switches[i] ? frac[i] : 0.0;
            gains[i] = p.nodeGain * a * attenuation[i] * (1.0 / rSrc[i]) / gTotal;
        }
    }

    MixerParams p;
    std::array<double, MixerParams::numChannels> pots {}, gains {}, rSource {}, attenuation {};
    std::array<bool, MixerParams::numChannels> switches {};
};

} // namespace mm
