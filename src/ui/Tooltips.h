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
// Hover help for the panel controls (English): tooltips, and the switches'
// help bubbles.

#include <juce_core/juce_core.h>

namespace mm::ui
{

inline juce::String tooltipFor (const juce::String& id)
{
    static const std::pair<const char*, const char*> table[] = {
        // Controllers
        { "tune", "TUNE: master tuning of the oscillators (about +/-2.5 semitones). Centre = A-440." },
        { "glide", "GLIDE: how fast the pitch slides to a new key, from instantly to about a second an octave. The slide stops where it is when the last key is released. Active only when the GLIDE switch is on." },
        { "modMix", "MODULATION MIX: blends the modulation source, from oscillator 3 (0) to noise (10): pink noise, or red noise when the noise switch is on PINK. Depth is set by the MOD wheel." },

        // Oscillator bank
        { "osc1Range", "Oscillator 1 RANGE: octave footage (32' to 2'), or LO for sub-audio frequencies." },
        { "osc2Range", "Oscillator 2 RANGE: octave footage (32' to 2'), or LO for sub-audio frequencies." },
        { "osc3Range", "Oscillator 3 RANGE: octave footage (32' to 2'), or LO to use it as a low-frequency modulation source." },
        { "osc2Freq", "Oscillator 2 FREQUENCY: detunes oscillator 2 relative to oscillator 1, about 8 semitones each way over the dial. Small offsets give the classic thick beating sound." },
        { "osc3Freq", "Oscillator 3 FREQUENCY: detunes oscillator 3, about 8 semitones each way. With OSC. 3 CONTROL off its range is over four times wider (6 octaves): it sets the modulation rate." },
        { "osc1Wave", "Oscillator 1 WAVEFORM: triangle, shark-tooth, sawtooth, square, wide or narrow rectangle." },
        { "osc2Wave", "Oscillator 2 WAVEFORM: triangle, shark-tooth, sawtooth, square, wide or narrow rectangle." },
        { "osc3Wave", "Oscillator 3 WAVEFORM: triangle, reverse sawtooth, sawtooth, square, wide or narrow rectangle." },

        // Mixer
        { "osc1Vol", "Oscillator 1 level into the filter. High levels from several sources overdrive the filter input." },
        { "osc2Vol", "Oscillator 2 level into the filter. High levels from several sources overdrive the filter input." },
        { "osc3Vol", "Oscillator 3 level into the filter. High levels from several sources overdrive the filter input." },
        { "extVol", "EXTERNAL INPUT VOLUME: level of the plugin's audio input and/or the FEEDBACK patch into the external input pre-amplifier (gain of about 200). Watch the OVERLOAD lamp." },
        { "noiseVol", "NOISE level into the filter." },
        { "overload", "Lights when the external input pre-amplifier output exceeds about 2 V peak (board 4 lamp driver), i.e. when the external signal or the FEEDBACK patch drives the filter harder than an oscillator at full volume. Lower EXTERNAL INPUT VOLUME to turn it off." },

        // Modifiers
        { "cutoff", "CUTOFF FREQUENCY of the 24 dB/octave transistor ladder low-pass filter: an octave a mark, about 28 Hz to 28 kHz over the dial, 440 Hz at -1 (KEYBOARD CONTROL off)." },
        { "emphasis", "EMPHASIS: filter resonance. The filter starts to self-oscillate (sine whistle) at about 7-8." },
        { "contourAmt", "AMOUNT OF CONTOUR: how far the filter contour opens the cutoff. At 10 the contour's peak takes the filter to the top of its range." },
        { "fAttack", "Filter contour ATTACK TIME: time to reach the peak, from about 1 ms to 10 s." },
        { "fDecay", "Filter contour DECAY TIME: time constant of the decay, from about 1 ms to 10 s (the decay is complete after about 3.5 times this). Also the release time when the DECAY switch is on." },
        { "fSustain", "Filter contour SUSTAIN LEVEL held while a key is down." },
        { "lAttack", "Loudness contour ATTACK TIME: time to reach the peak, from about 1 ms to 10 s." },
        { "lDecay", "Loudness contour DECAY TIME: time constant of the decay, from about 1 ms to 10 s (the decay is complete after about 3.5 times this). Also the release time when the DECAY switch is on." },
        { "lSustain", "Loudness contour SUSTAIN LEVEL held while a key is down." },

        // Output and left-hand controller
        { "volume", "Main output VOLUME: a passive 5k linear pot after the output amplifier (about -6 dB at 5)." },

        // Added settings
        { "voiceMode", "MONO plays one note at a time, as the original (lowest-note priority, single trigger); POLY plays several." },
        { "voices", "Number of voices in POLY mode (2 to 8). Each voice is a separately drawn and calibrated unit." },
        { "quality", "Oversampling of the circuit simulation (2x, 4x or 8x). Higher = less aliasing, more CPU." },
        { "drift", "Thermal wandering of the circuits (0 = lab-stable, 1 = typical vintage unit, 3 = hot stage)." },
        { "unitSeed", "Selects which example of the instrument you play (different component tolerances, recalibrated)." },
        { "bendRange", "Pitch wheel range in semitones. The original's wheel bends about 7 each way." },
        { "modAmount", "Modulation depth stored with the sound, like a MOD. wheel left in position. The wheel can only add to it (the deeper of the two is used)." },
    };
    for (const auto& [key, text] : table)
        if (id == key)
            return text;
    return {};
}

/** A switch's help bubble: "<name>: <state>", then the description. */
struct SwitchHelp
{
    const char* name;
    const char* off; // state shown when the switch is off (toggle state false)
    const char* on;
    const char* description;
};

inline SwitchHelp switchHelpFor (const juce::String& id)
{
    static const std::pair<const char*, SwitchHelp> table[] = {
        // Controllers
        { "oscMod", { "OSCILLATOR MODULATION", "OFF", "ON", "Routes the modulation mix to the pitch of oscillators 1 and 2 (vibrato, trills, FM)." } },
        { "modSourceA", { "OSC. 3 / FILTER EG", "INACTIVE", "INACTIVE", "This switch only exists on the modern reissues of the Model D, not on the original instrument. Shown for reference; it has no effect." } },
        { "modSourceB", { "NOISE / LFO", "INACTIVE", "INACTIVE", "This switch only exists on the modern reissues of the Model D, not on the original instrument. Shown for reference; it has no effect." } },
        { "osc3Control", { "OSC. 3 CONTROL", "OFF", "ON", "On: oscillator 3 follows the keyboard, the pitch wheel, TUNE and the modulation. Off: it runs free, with a much wider FREQUENCY knob: typically a modulation source in the LO range." } },

        // Mixer
        { "osc1On", { "OSCILLATOR 1", "OFF", "ON", "Switches oscillator 1 into the mixer." } },
        { "osc2On", { "OSCILLATOR 2", "OFF", "ON", "Switches oscillator 2 into the mixer." } },
        { "osc3On", { "OSCILLATOR 3", "OFF", "ON", "Switches oscillator 3 into the mixer. It can still modulate when switched off here." } },
        { "extOn", { "EXTERNAL INPUT", "OFF", "ON", "Switches the external input into the mixer." } },
        { "noiseOn", { "NOISE", "OFF", "ON", "Switches the noise into the mixer." } },
        { "noiseColor", { "NOISE COLOUR", "WHITE", "PINK", "Double switch, as on the original. WHITE (up): white noise to the mixer, pink noise to the MODULATION MIX. PINK (down): pink noise (-3 dB/octave) to the mixer, red noise (slower, darker) to the MODULATION MIX." } },

        // Modifiers
        { "filterMod", { "FILTER MODULATION", "OFF", "ON", "Routes the modulation mix to the filter cutoff (wah, growl, sample-and-hold-like effects with noise)." } },
        { "kbd1", { "KEYBOARD CONTROL 1", "OFF", "ON", "Upper switch: the cutoff follows the keyboard by 1/3. With switch 2 also on, it tracks 1:1." } },
        { "kbd2", { "KEYBOARD CONTROL 2", "OFF", "ON", "Lower switch: the cutoff follows the keyboard by 2/3. With switch 1 also on, it tracks 1:1." } },

        // Output and left-hand controller
        { "mainOn", { "MAIN OUTPUT", "OFF", "ON", "Switches the main output on or off." } },
        { "a440", { "A-440", "OFF", "ON", "Plays a 440 Hz reference tone for tuning against other instruments. Its level follows the VOLUME knob and the ON switch." } },
        { "glideOn", { "GLIDE", "OFF", "ON", "Enables the glide (rate set by the GLIDE knob)." } },
        { "decayOn", { "DECAY", "OFF", "ON", "On: notes release with the DECAY times. Off: notes stop almost immediately on key release." } },

        // Added settings
        { "feedback", { "FEEDBACK", "OFF", "ON", "Patches the output back into the EXTERNAL INPUT (the classic overdrive trick). Its level is set by the EXTERNAL INPUT VOLUME." } },
    };
    for (const auto& [key, help] : table)
        if (id == key)
            return help;
    return { "", "OFF", "ON", "" };
}

} // namespace mm::ui
