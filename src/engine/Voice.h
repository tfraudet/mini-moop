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
// their expo converters, noise, mixer, ladder filter with its cutoff converter,
// two contour generators, VCA and modulation bus. Every voice draws its own
// component tolerances from its seed, is calibrated as the service manual
// prescribes (Voice.cpp), and drifts with its own thermal model.

#include "../circuit/blocks/ContourGen.h"
#include "../circuit/blocks/CutoffConverter.h"
#include "../circuit/blocks/ExpoConverter.h"
#include "../circuit/blocks/ExtPreamp.h"
#include "../circuit/blocks/Glide.h"
#include "../circuit/blocks/LadderFilter.h"
#include "../circuit/blocks/Mixer.h"
#include "../circuit/blocks/ModulationMix.h"
#include "../circuit/blocks/NoiseSource.h"
#include "../circuit/blocks/OutputVolume.h"
#include "../circuit/blocks/TriggerSection.h"
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
    /** `a440`: the A-440 oscillator's output (V) at the oversampled rate, into
        this unit's VCA; null when off or for the other units of a poly set. */
    void render (const PanelState& panel, double* out, const double* extIn, int n, const double* a440 = nullptr) noexcept;

    // Diagnostics / tests
    double oscFrequency (int osc, double cv) noexcept;
    double filterCutoffHz (double cv) noexcept;
    /** The frequency (Hz) the filter sings at for a control voltage in octaves
        (EMPHASIS 10, no input), measured on a copy of the filter. */
    double filterSelfOscillationHz (double cv);
    /** Whether the filter builds up an oscillation at an EMPHASIS position 0..1. */
    bool filterRegenerates (double cv, double emphasis);
    /** An oscillator's frequency (Hz) for a panel state and a key, at 25 C,
        without modulation. */
    double oscFrequencyFor (int osc, const PanelState& panel, int note) noexcept;
    /** The filter's control voltage (octaves) for a panel state, a key and a
        filter contour voltage. */
    double filterCv (const PanelState& panel, int note, double contourVolts) const noexcept;
    /** Octaves above 8' of an oscillator's RANGE position on this unit. */
    double rangeOctaves (int osc, Range r) const noexcept { return rangeCv[size_t (osc)][size_t (r)]; }
    /** The keyboard's voltage (V, 0 on the lowest key) for a MIDI note on this unit. */
    double keyVolts (int note) const noexcept { return keyCv[size_t (note < 0 ? 0 : note > 127 ? 127 : note)]; }
    const LadderFilter& filter() const noexcept { return ladder; }
    const Vca& amplifier() const noexcept { return vca; }
    double noiseLevel() const noexcept { return whiteLevel; }
    /** TUNE's span on this unit, octaves over the whole travel. */
    double tuneRange() const noexcept { return tuneSpan; }
    /** Volts at the main output jack for a volt at the output stage, VOLUME at 10. */
    static double jackGain() noexcept { return OutputVolume::gain (1.0); }
    const VcoCore& vco (int i) const noexcept { return vcos[size_t (i)]; }
    double lastOutput() const noexcept { return lastOut; }
    /** OVERLOAD lamp brightness of this unit, 0..1. */
    double overloadLamp() const noexcept { return extPreamp.lampLevel(); }

private:
    void drawPanel (Rng& rng);
    void calibrateOscillators();
    void calibrateFilter();
    void calibrateNoise();
    void controlUpdate (const PanelState& panel) noexcept;
    double oscPanelCv (int osc, const PanelState& panel) const noexcept;
    double osc3FreeCvFor (const PanelState& panel) const noexcept;

    // Circuits
    std::array<ExpoConverter, 3> vcoExpo;
    std::array<VcoCore, 3> vcos;
    CutoffConverter filterExpo;
    LadderFilter ladder;
    NoiseSource noise;
    Mixer mixer;
    ExtPreamp extPreamp;
    ContourGen filterContour, loudnessContour;
    TriggerSection trigger;     // the contours' trigger section: C7's delay on both edges
    bool vTrigger = false;      // its output, the V-trigger
    Vca vca;
    Glide glide;
    DriftModel drift;

    // This unit's panel networks (drawn in build, see FrontPanel.h)
    std::array<std::array<double, 6>, 3> rangeCv {}; // octaves above 8' of each RANGE position, after the OCTAVE trim
    std::array<double, 128> keyCv {};                // keyboard volts per MIDI note (the 43-resistor string)
    double tuneSpan = 0.0;                           // TUNE, octaves over the whole travel
    std::array<double, 3> freqSpan {};               // FREQUENCY (oscillators 2, 3), octaves over the whole travel
    double kbd1Gain = 1.0 / 3.0, kbd2Gain = 2.0 / 3.0; // KEYBOARD CONTROL 1 / 2, octaves per keyboard volt
    double cutoffGain = 0.5, contourGain = 2.128, filterModGain = 3.03; // octaves per volt at the control node
    double whiteLevel = 0.1;                         // NOISE LEVEL trim: the white noise's level (V rms)

    uint64_t seed = 1;
    double fs = 192000.0;
    int targetNote = 60;
    bool gate = false, firstNote = true;
    int controlCounter = 0;
    double lastOut = 0.0, feedbackSample = 0.0;
    double osc3Last = 0.0;

    // Control-rate snapshot
    std::array<double, 3> oscStaticCv {};
    double filterStaticCv = 0.0, kbTrack = 0.0, contourOct = 0.0, osc3FreeCv = 0.0;
    double oscModDepth = 0.0, filterModDepth = 0.0, volume = 0.0;
    double modOsc3 = 1.0, modNoise = 0.0; // MODULATION MIX weights
    double modFloor = -1.7;               // the modulation amplifier's lowest output (V), for the line's load
    double modOffset = 0.0;               // and its standing offset (V)
    bool osc3Kbd = true, pink = false, feedback = false;
};

} // namespace mm
