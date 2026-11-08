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
// Hover help for every panel control (English).

#include <juce_core/juce_core.h>

namespace mm::ui
{

inline juce::String tooltipFor (const juce::String& id)
{
    static const std::pair<const char*, const char*> table[] = {
        // Controllers
        { "tune", "TUNE: master tuning of all three oscillators (+/-1.5 semitones). Centre = A-440." },
        { "glide", "GLIDE: portamento time between notes (about 1 ms to 1 s per octave). Active only when the GLIDE switch is on." },
        { "modMix", "MODULATION MIX: blends the modulation source, from oscillator 3 (0) to noise or filter contour (10). Depth is set by the MOD wheel." },
        { "modSource", "Second modulation source for MODULATION MIX: NOISE (up) or FILTER CONTOUR (down)." },
        { "oscMod", "OSCILLATOR MODULATION: routes the modulation mix to the pitch of oscillators 1 and 2 (vibrato, trills, FM)." },
        { "osc3Control", "OSC. 3 CONTROL: on = oscillator 3 follows the keyboard; off = oscillator 3 is free-running, typically used as an LFO in the LO range." },

        // Oscillator bank
        { "osc1Range", "Oscillator 1 RANGE: octave footage (32' to 2'), or LO for sub-audio frequencies." },
        { "osc2Range", "Oscillator 2 RANGE: octave footage (32' to 2'), or LO for sub-audio frequencies." },
        { "osc3Range", "Oscillator 3 RANGE: octave footage (32' to 2'), or LO to use it as a low-frequency modulation source." },
        { "osc2Freq", "Oscillator 2 FREQUENCY: detunes oscillator 2 relative to oscillator 1 (+/-7 semitones). Small offsets give the classic thick beating sound." },
        { "osc3Freq", "Oscillator 3 FREQUENCY: detunes oscillator 3 (+/-7 semitones), or sets the LFO rate when used as a modulator." },
        { "osc1Wave", "Oscillator 1 WAVEFORM: triangle, shark-tooth, sawtooth, square, wide or narrow rectangle." },
        { "osc2Wave", "Oscillator 2 WAVEFORM: triangle, shark-tooth, sawtooth, square, wide or narrow rectangle." },
        { "osc3Wave", "Oscillator 3 WAVEFORM: triangle, reverse sawtooth, sawtooth, square, wide or narrow rectangle." },

        // Mixer
        { "osc1Vol", "Oscillator 1 level into the filter. High levels from several sources overdrive the filter input." },
        { "osc2Vol", "Oscillator 2 level into the filter. High levels from several sources overdrive the filter input." },
        { "osc3Vol", "Oscillator 3 level into the filter. High levels from several sources overdrive the filter input." },
        { "extVol", "EXTERNAL INPUT level: the plugin's audio input and/or the FEEDBACK patch, mixed into the filter." },
        { "noiseVol", "NOISE level into the filter." },
        { "osc1On", "Oscillator 1 on/off in the mixer." },
        { "osc2On", "Oscillator 2 on/off in the mixer." },
        { "osc3On", "Oscillator 3 on/off in the mixer (it can still modulate when switched off here)." },
        { "extOn", "External input on/off in the mixer." },
        { "noiseOn", "Noise on/off in the mixer." },
        { "noiseColor", "Noise colour: WHITE (up) or PINK (down, -3 dB/octave, darker)." },

        // Modifiers
        { "filterMod", "FILTER MODULATION: routes the modulation mix to the filter cutoff (wah, growl, sample-and-hold-like effects with noise)." },
        { "cutoff", "CUTOFF FREQUENCY of the 24 dB/octave transistor ladder low-pass filter (about 14 Hz to 20 kHz)." },
        { "emphasis", "EMPHASIS: filter resonance. The filter starts to self-oscillate (sine whistle) at about 7-8." },
        { "contourAmt", "AMOUNT OF CONTOUR: how far the filter contour opens the cutoff (0 to 4 octaves)." },
        { "kbd1", "KEYBOARD CONTROL 1: cutoff follows the keyboard by 1/3. With switch 2 also on, it tracks 1:1." },
        { "kbd2", "KEYBOARD CONTROL 2: cutoff follows the keyboard by 2/3. With switch 1 also on, it tracks 1:1." },
        { "fAttack", "Filter contour ATTACK TIME (1 ms to 10 s)." },
        { "fDecay", "Filter contour DECAY TIME (4 ms to 35 s). Also the release time when the DECAY switch is on." },
        { "fSustain", "Filter contour SUSTAIN LEVEL held while a key is down." },
        { "lAttack", "Loudness contour ATTACK TIME (1 ms to 10 s)." },
        { "lDecay", "Loudness contour DECAY TIME (4 ms to 35 s). Also the release time when the DECAY switch is on." },
        { "lSustain", "Loudness contour SUSTAIN LEVEL held while a key is down." },

        // Output and left-hand controller
        { "volume", "Main output VOLUME." },
        { "mainOn", "Main output on/off." },
        { "a440", "A-440: plays a 440 Hz reference tone for tuning against other instruments." },
        { "glideOn", "GLIDE switch: enables portamento (time set by the GLIDE knob)." },
        { "decayOn", "DECAY switch: on = notes release with the DECAY times; off = notes stop almost immediately on key release." },

        // Added settings
        { "voiceMode", "Voice mode: MONO (original behaviour: lowest-note priority, single trigger) or POLY." },
        { "voices", "Number of voices in POLY mode (2 to 8). Each voice is a separately drawn and calibrated unit." },
        { "quality", "Oversampling of the circuit simulation: Eco 2x, High 4x, Ultra 8x. Higher = less aliasing, more CPU." },
        { "drift", "Analog drift: thermal wandering of the circuits (0 = lab-stable, 1 = typical vintage unit, 3 = hot stage)." },
        { "unitSeed", "Unit: selects which example of the instrument you play (different component tolerances, recalibrated)." },
        { "bendRange", "Pitch wheel range in semitones." },
        { "feedback", "FEEDBACK patch: connects the output back into the EXTERNAL INPUT (the classic overdrive trick). Level is set by the EXT. INPUT volume." },
    };
    for (const auto& [key, text] : table)
        if (id == key)
            return text;
    return {};
}

} // namespace mm::ui
