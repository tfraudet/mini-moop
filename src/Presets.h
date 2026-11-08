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
// Factory programs. Values are in parameter units (pots 0..10, choices by index,
// switches 0/1). Anything not listed is reset to its default.

#include <initializer_list>
#include <utility>

namespace mm
{

struct Preset
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

// Waveform indices: 0 tri, 1 shark/rev-saw, 2 saw, 3 square, 4 wide, 5 narrow
// Range indices:    0 LO, 1 32', 2 16', 3 8', 4 4', 5 2'
inline const Preset factoryPresets[] = {
    { "Init", {} },
    { "Fat Bass", {
        { "osc1Range", 1 }, { "osc1Wave", 2 }, { "osc1Vol", 10 },
        { "osc2Range", 1 }, { "osc2Wave", 2 }, { "osc2Freq", 0.08f }, { "osc2Vol", 8 }, { "osc2On", 1 },
        { "osc3Range", 2 }, { "osc3Wave", 4 }, { "osc3Freq", -0.06f }, { "osc3Vol", 6 }, { "osc3On", 1 },
        { "cutoff", 3.5f }, { "emphasis", 3 }, { "contourAmt", 6 },
        { "fAttack", 0 }, { "fDecay", 3.5f }, { "fSustain", 2 },
        { "lAttack", 0 }, { "lDecay", 3 }, { "lSustain", 8 }, { "kbd1", 1 }, { "kbd2", 0 } } },
    { "Solo Lead", {
        { "osc1Range", 3 }, { "osc1Wave", 2 }, { "osc2Range", 3 }, { "osc2Wave", 3 }, { "osc2Freq", 0.1f },
        { "osc2On", 1 }, { "osc2Vol", 8 }, { "glide", 3 }, { "glideOn", 1 },
        { "cutoff", 5 }, { "emphasis", 5 }, { "contourAmt", 4 }, { "fDecay", 5 }, { "fSustain", 6 },
        { "lDecay", 5 }, { "lSustain", 10 }, { "decayOn", 1 }, { "lAttack", 0.5f },
        { "oscMod", 1 }, { "osc3Control", 0 }, { "osc3Range", 0 }, { "osc3Wave", 0 }, { "osc3Freq", 2 } } },
    { "Self-Osc Whistle", {
        { "osc1On", 0 }, { "emphasis", 9.5f }, { "cutoff", 4 }, { "kbd1", 1 }, { "kbd2", 1 },
        { "contourAmt", 0 }, { "lDecay", 5 }, { "lSustain", 10 } } },
    { "Feedback Growl", {
        { "osc1Range", 2 }, { "osc1Wave", 2 }, { "osc2Range", 2 }, { "osc2Wave", 5 }, { "osc2On", 1 }, { "osc2Vol", 7 },
        { "feedback", 1 }, { "extOn", 1 }, { "extVol", 6 },
        { "cutoff", 4 }, { "emphasis", 4 }, { "contourAmt", 5 }, { "fDecay", 4 }, { "fSustain", 3 } } },
    { "Osc 3 Wobble", {
        { "osc1Range", 2 }, { "osc1Wave", 2 }, { "osc2Range", 2 }, { "osc2Wave", 2 }, { "osc2Freq", -0.1f }, { "osc2On", 1 },
        { "osc3Range", 0 }, { "osc3Wave", 0 }, { "osc3Control", 0 }, { "osc3Freq", 3 }, { "osc3On", 0 },
        { "filterMod", 1 }, { "modMix", 0 }, { "cutoff", 3.5f }, { "emphasis", 6 } } },
    { "Poly Brass", {
        { "voiceMode", 1 }, { "voices", 6 },
        { "osc1Wave", 2 }, { "osc2Wave", 2 }, { "osc2Freq", 0.12f }, { "osc2On", 1 }, { "osc2Vol", 9 },
        { "cutoff", 3 }, { "emphasis", 1 }, { "contourAmt", 6 },
        { "fAttack", 4.5f }, { "fDecay", 5 }, { "fSustain", 5 },
        { "lAttack", 3 }, { "lDecay", 6 }, { "lSustain", 9 }, { "decayOn", 1 } } },
    { "Noise Wind", {
        { "osc1On", 0 }, { "noiseOn", 1 }, { "noiseVol", 9 }, { "noiseColor", 1 },
        { "cutoff", 4 }, { "emphasis", 8 }, { "oscMod", 0 }, { "filterMod", 1 },
        { "osc3Range", 0 }, { "osc3Wave", 0 }, { "osc3Control", 0 }, { "osc3Freq", -5 },
        { "lAttack", 6 }, { "lDecay", 7 }, { "decayOn", 1 } } },
};

inline constexpr int numFactoryPresets = int (sizeof (factoryPresets) / sizeof (factoryPresets[0]));

} // namespace mm
