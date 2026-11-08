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

#include "../components/Passives.h"

#include <array>

namespace mm
{

struct MixerParams
{
    static constexpr int numChannels = 5; // osc1, ext, osc2, noise, osc3

    double rPot = 10.0e3;                   // volume pots
    std::array<double, numChannels> rSum { 47e3, 47e3, 47e3, 47e3, 47e3 }; // summing resistors
    double rNode = 4.7e3;                   // summing node to ground (input attenuator)
    double nodeGain = 1.0;                  // buffer gain to filter input

    static MixerParams drawn (Rng& rng, const MixerParams& nominal) noexcept
    {
        MixerParams p = nominal;
        for (auto& r : p.rSum) r = rng.tolerance (r, 0.05);
        p.rNode = rng.tolerance (nominal.rNode, 0.05);
        p.rPot = rng.tolerance (nominal.rPot, 0.20);
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
        std::array<double, MixerParams::numChannels> rSrc {};
        for (size_t i = 0; i < rSrc.size(); ++i)
        {
            const double a = taper::audio (pots[i]);
            const double rWiper = a * (1.0 - a) * p.rPot; // Thevenin of the pot divider
            rSrc[i] = p.rSum[i] + rWiper;
            gTotal += 1.0 / rSrc[i];
        }
        for (size_t i = 0; i < rSrc.size(); ++i)
        {
            const double a = switches[i] ? taper::audio (pots[i]) : 0.0;
            gains[i] = p.nodeGain * a * (1.0 / rSrc[i]) / gTotal;
        }
    }

    MixerParams p;
    std::array<double, MixerParams::numChannels> pots {}, gains {};
    std::array<bool, MixerParams::numChannels> switches {};
};

} // namespace mm
