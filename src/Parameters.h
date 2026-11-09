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

#include "engine/PanelState.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace mm::params
{

// Parameter IDs
namespace id
{
inline constexpr const char* tune = "tune";
inline constexpr const char* glide = "glide";
inline constexpr const char* modMix = "modMix";
inline constexpr const char* oscMod = "oscMod";
inline constexpr const char* filterMod = "filterMod";
inline constexpr const char* osc3Control = "osc3Control";
inline constexpr const char* oscRange[] = { "osc1Range", "osc2Range", "osc3Range" };
inline constexpr const char* oscWave[] = { "osc1Wave", "osc2Wave", "osc3Wave" };
inline constexpr const char* oscFreq[] = { "", "osc2Freq", "osc3Freq" };
inline constexpr const char* oscVol[] = { "osc1Vol", "osc2Vol", "osc3Vol" };
inline constexpr const char* oscOn[] = { "osc1On", "osc2On", "osc3On" };
inline constexpr const char* extVol = "extVol";
inline constexpr const char* extOn = "extOn";
inline constexpr const char* noiseVol = "noiseVol";
inline constexpr const char* noiseOn = "noiseOn";
inline constexpr const char* noiseColor = "noiseColor";
inline constexpr const char* cutoff = "cutoff";
inline constexpr const char* emphasis = "emphasis";
inline constexpr const char* contourAmt = "contourAmt";
inline constexpr const char* kbd1 = "kbd1";
inline constexpr const char* kbd2 = "kbd2";
inline constexpr const char* fAttack = "fAttack";
inline constexpr const char* fDecay = "fDecay";
inline constexpr const char* fSustain = "fSustain";
inline constexpr const char* lAttack = "lAttack";
inline constexpr const char* lDecay = "lDecay";
inline constexpr const char* lSustain = "lSustain";
inline constexpr const char* volume = "volume";
inline constexpr const char* mainOn = "mainOn";
inline constexpr const char* a440 = "a440";
inline constexpr const char* glideOn = "glideOn";
inline constexpr const char* decayOn = "decayOn";
inline constexpr const char* feedback = "feedback";
inline constexpr const char* voiceMode = "voiceMode";
inline constexpr const char* voices = "voices";
inline constexpr const char* quality = "quality";
inline constexpr const char* drift = "drift";
inline constexpr const char* unitSeed = "unitSeed";
inline constexpr const char* bendRange = "bendRange";
} // namespace id

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Cached raw pointers for lock-free reads on the audio thread. */
class ParameterCache
{
public:
    void attach (juce::AudioProcessorValueTreeState& state);
    /** Builds the circuit-side panel state from the current parameter values. */
    void read (PanelState& p) const noexcept;

    int voiceMode() const noexcept { return int (voiceModeP->load()); }
    int voices() const noexcept { return int (voicesP->load()); }
    int oversampling() const noexcept;
    int unitSeed() const noexcept { return int (unitSeedP->load()); }

private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const char* pid);

    std::atomic<float> *tune {}, *glide {}, *modMix {}, *oscMod {}, *filterMod {}, *osc3Control {};
    std::atomic<float> *oscRange[3] {}, *oscWave[3] {}, *oscFreq[3] {}, *oscVol[3] {}, *oscOn[3] {};
    std::atomic<float> *extVol {}, *extOn {}, *noiseVol {}, *noiseOn {}, *noiseColor {};
    std::atomic<float> *cutoff {}, *emphasis {}, *contourAmt {}, *kbd1 {}, *kbd2 {};
    std::atomic<float> *fAttack {}, *fDecay {}, *fSustain {}, *lAttack {}, *lDecay {}, *lSustain {};
    std::atomic<float> *volume {}, *mainOn {}, *a440 {}, *glideOn {}, *decayOn {}, *feedback {};
    std::atomic<float> *voiceModeP {}, *voicesP {}, *qualityP {}, *drift {}, *unitSeedP {}, *bendRange {};
};

} // namespace mm::params
