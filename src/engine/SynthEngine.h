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
// Top-level engine: voice allocation, oversampled rendering of the voices,
// decimation back to the host rate, output coupling capacitor, A-440
// reference. Pure C++ (no JUCE), real-time safe after prepare().

#include "Decimator.h"
#include "PanelState.h"
#include "Voice.h"
#include "VoiceAllocator.h"

#include <array>
#include <vector>

namespace mm
{

class SynthEngine
{
public:
    static constexpr int maxVoices = VoiceAllocator::maxVoices;

    /** Builds the voices ("units") from a seed. Not real-time: call from prepare
        or when the user changes the unit seed. */
    void build (uint64_t unitSeed);

    void prepare (double hostSampleRate, int maxBlockSize, int oversampling);
    int oversampling() const noexcept { return osFactor; }

    /** New target panel state; continuous controls are smoothed (~10 ms). */
    void setPanel (const PanelState& p) noexcept { target = p; if (! panelInitialised) { panel = p; panelInitialised = true; } }
    const PanelState& getPanel() const noexcept { return panel; }
    void setVoiceMode (bool poly, int voices) noexcept;

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void sustainPedal (bool down) noexcept { alloc.setSustainPedal (down); }
    void allNotesOff() noexcept;

    /** Renders `n` host samples (mono) into `out`. `in` may be null. */
    void process (float* out, const float* in, int n) noexcept;

    Voice& voice (int i) noexcept { return voices[size_t (i)]; }
    static constexpr double outputScale = 0.25; // volts -> digital full scale (+/-4 V)

private:
    void apply (const VoiceCommand& c) noexcept;

    std::array<Voice, maxVoices> voices;
    VoiceAllocator alloc;
    Decimator decimator;
    PanelState panel, target;
    bool panelInitialised = false;
    double smoothCoeff = 0.1;
    static constexpr int smoothChunk = 16; // host samples per smoothing step
    std::vector<double> osBuffer, extBuffer;
    double hostRate = 48000.0;
    int osFactor = 4, maxBlock = 512;
    double dcState = 0.0, dcPrevIn = 0.0, dcCoeff = 0.9995;
    double refPhase = 0.0;
    float lastIn = 0.0f;
};

} // namespace mm
