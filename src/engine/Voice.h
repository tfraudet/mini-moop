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
// One complete instrument voice: keyboard CV + glide, three oscillators with
// their expo converters, noise, mixer, ladder filter with its expo converter,
// two contour generators, VCA and modulation bus. Every voice draws its own
// component tolerances from its seed, is calibrated like a real unit, and
// drifts with its own thermal model.

#include "../circuit/blocks/ContourGen.h"
#include "../circuit/blocks/ExpoConverter.h"
#include "../circuit/blocks/ExtPreamp.h"
#include "../circuit/blocks/Glide.h"
#include "../circuit/blocks/LadderFilter.h"
#include "../circuit/blocks/Mixer.h"
#include "../circuit/blocks/NoiseSource.h"
#include "../circuit/blocks/OutputVolume.h"
#include "../circuit/blocks/Vca.h"
#include "../circuit/blocks/VcoCore.h"
#include "DriftModel.h"
#include "PanelState.h"

#include <array>
#include <cstdint>

namespace mm
{

class Voice
{
public:
    static constexpr int controlInterval = 32; // samples between control-rate updates

    /** Draws the component values for this "unit" and calibrates it. */
    void build (uint64_t seed);

    void prepare (double oversampledRate);
    void reset();

    void noteOn (int note, bool retrigger) noexcept;
    void noteOff() noexcept;
    void setNote (int note) noexcept { targetNote = note; }
    bool isActive() const noexcept;

    /** Render `n` samples at the oversampled rate, adding into `out` (volts).
        `extIn` is the external input signal (volts) or nullptr. */
    void render (const PanelState& panel, double* out, const double* extIn, int n) noexcept;

    // Diagnostics / tests
    double oscFrequency (int osc, double cv) noexcept;
    double filterCutoffHz (double cv) noexcept;
    const VcoCore& vco (int i) const noexcept { return vcos[size_t (i)]; }
    double lastOutput() const noexcept { return lastOut; }
    /** OVERLOAD lamp brightness of this unit, 0..1. */
    double overloadLamp() const noexcept { return extPreamp.lampLevel(); }

private:
    void calibrateOscillators();
    void calibrateFilter();
    void controlUpdate (const PanelState& panel) noexcept;

    // Circuits
    std::array<ExpoConverter, 3> vcoExpo;
    std::array<VcoCore, 3> vcos;
    ExpoConverter filterExpo;
    LadderFilter ladder;
    NoiseSource noise;
    Mixer mixer;
    ExtPreamp extPreamp;
    ContourGen filterContour, loudnessContour;
    Vca vca;
    Glide glide;
    DriftModel drift;

    // Calibrated references
    double filterI0 = 2.8e-5;  // tail current at filter CV 0 (f0)
    static constexpr double filterF0 = 640.0;

    uint64_t seed = 1;
    double fs = 192000.0;
    int targetNote = 60;
    bool gate = false, firstNote = true;
    int controlCounter = 0;
    double lastOut = 0.0, feedbackSample = 0.0;
    double osc3Last = 0.0;

    // Control-rate snapshot
    std::array<double, 3> oscStaticCv {};
    double filterStaticCv = 0.0, kbTrack = 0.0, contourOct = 0.0;
    double oscModDepth = 0.0, filterModDepth = 0.0, modMix = 0.0, volume = 0.0;
    bool osc3Kbd = true, pink = false, feedback = false;
};

} // namespace mm
