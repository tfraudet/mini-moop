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

#include "Parameters.h"

#include "circuit/blocks/ContourGen.h"
#include "engine/PanelLaws.h"

namespace mm::params
{

namespace
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;

/** Panel pots are labelled 0..10; the circuits receive 0..1. */
std::unique_ptr<APF> pot (const char* pid, const juce::String& name, float def)
{
    return std::make_unique<APF> (juce::ParameterID { pid, 1 }, name,
                                  juce::NormalisableRange<float> (0.0f, 10.0f, 0.0f), def,
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); }));
}

/** "473Hz", "1.5kHz", "12.4kHz". */
juce::String formatHz (double hz)
{
    if (hz < 1000.0) return juce::String (juce::roundToInt (hz)) + "Hz";
    return juce::String (hz / 1000.0, 1) + "kHz";
}

/** CUTOFF FREQUENCY: a 0..10 pot internally (as stored in presets), shown
    -5..+5 as engraved on the original panel, followed by the frequency the
    filter sings at there (KEYBOARD CONTROL off): "-3.2 (1.5kHz)". */
std::unique_ptr<APF> cutoffPot (const char* pid, const juce::String& name, float def)
{
    return std::make_unique<APF> (juce::ParameterID { pid, 1 }, name,
                                  juce::NormalisableRange<float> (0.0f, 10.0f, 0.0f), def,
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction ([] (float v, int) {
                                          const float c = v - 5.0f;
                                          return (c > 0.04f ? "+" : "") + juce::String (c, 1)
                                                 + " (" + formatHz (laws::cutoffHz (double (v) / 10.0)) + ")";
                                      })
                                      .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() + 5.0f; }));
}

/** Centre-detented pots (TUNE, osc 2/3 FREQUENCY), labelled -1..+1 / semitones. */
std::unique_ptr<APF> bipolar (const char* pid, const juce::String& name, float span, const juce::String& unit)
{
    return std::make_unique<APF> (juce::ParameterID { pid, 1 }, name,
                                  juce::NormalisableRange<float> (-span, span, 0.0f), 0.0f,
                                  juce::AudioParameterFloatAttributes()
                                      .withLabel (unit)
                                      .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); }));
}

/** Rocker switches. Implemented as two-state choices rather than
    AudioParameterBool: a bool parameter reports the raw normalised value a
    host wrote (e.g. 0.48) until it is saved, so it does not survive a state
    round-trip unchanged. A choice quantises immediately. */
std::unique_ptr<APC> sw (const char* pid, const juce::String& name, bool def)
{
    return std::make_unique<APC> (juce::ParameterID { pid, 1 }, name, juce::StringArray { "Off", "On" }, def ? 1 : 0);
}

/** "165 ms", "4.83 s": milliseconds below one second, seconds above. */
juce::String formatTime (double seconds)
{
    if (seconds < 0.01) return juce::String (seconds * 1000.0, 1) + " ms";
    if (seconds < 1.0) return juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";
    if (seconds < 10.0) return juce::String (seconds, 2) + " s";
    return juce::String (seconds, 1) + " s";
}

/** ATTACK TIME / DECAY TIME pots: still 0..10 internally, but shown (and
    typed) as the time the circuit gives, as engraved on the panel. */
std::unique_ptr<APF> timePot (const char* pid, const juce::String& name, float def, bool attack, bool loudness)
{
    auto time = [attack, loudness] (double pos) {
        // The timing network's time constant: about the attack's 10-90 % time
        // (the peak comes after about 1.3 of them, see ContourGen.h).
        const auto nominal = loudness ? mm::ContourParams::loudness() : mm::ContourParams::filter();
        return attack ? mm::ContourGen::attackTime (nominal, pos) / mm::ContourGen::attackSpan (nominal)
                      : mm::ContourGen::decayTime (nominal, pos);
    };
    return std::make_unique<APF> (juce::ParameterID { pid, 1 }, name,
                                  juce::NormalisableRange<float> (0.0f, 10.0f, 0.0f), def,
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction ([time] (float v, int) { return formatTime (time (double (v) / 10.0)); })
                                      .withValueFromStringFunction ([time] (const juce::String& text) {
                                          // "200 ms", "1.5 s" or a bare number of seconds -> dial position
                                          const double number = text.retainCharacters ("0123456789.").getDoubleValue();
                                          const double target = text.containsIgnoreCase ("ms") ? number / 1000.0 : number;
                                          double lo = 0.0, hi = 1.0;
                                          for (int i = 0; i < 40; ++i)
                                          {
                                              const double mid = 0.5 * (lo + hi);
                                              (time (mid) < target ? lo : hi) = mid;
                                          }
                                          return float (10.0 * 0.5 * (lo + hi));
                                      }));
}

const juce::StringArray rangeNames { "LO", "32'", "16'", "8'", "4'", "2'" };
const juce::StringArray waveNames12 { "Triangle", "Shark tooth", "Sawtooth", "Square", "Wide rectangle", "Narrow rectangle" };
const juce::StringArray waveNames3 { "Triangle", "Reverse sawtooth", "Sawtooth", "Square", "Wide rectangle", "Narrow rectangle" };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Controllers
    l.add (bipolar (id::tune, "Tune", 1.5f, "st"));
    l.add (pot (id::glide, "Glide", 0.0f));
    l.add (pot (id::modMix, "Modulation Mix", 0.0f));
    l.add (sw (id::oscMod, "Oscillator Modulation", false));
    l.add (sw (id::filterMod, "Filter Modulation", false));
    l.add (sw (id::osc3Control, "Osc 3 Control", true));

    // Oscillator bank
    for (int i = 0; i < 3; ++i)
    {
        const auto n = juce::String (i + 1);
        l.add (std::make_unique<APC> (juce::ParameterID { id::oscRange[i], 1 }, "Osc " + n + " Range", rangeNames, 3));
        l.add (std::make_unique<APC> (juce::ParameterID { id::oscWave[i], 1 }, "Osc " + n + " Waveform",
                                      i == 2 ? waveNames3 : waveNames12, i == 2 ? 0 : 2));
        if (i > 0)
            l.add (bipolar (id::oscFreq[i], "Osc " + n + " Frequency", 7.0f, "st"));
        l.add (pot (id::oscVol[i], "Osc " + n + " Volume", i == 0 ? 10.0f : 6.0f));
        l.add (sw (id::oscOn[i], "Osc " + n + " On", i == 0));
    }

    // Mixer
    l.add (pot (id::extVol, "External Input Volume", 0.0f));
    l.add (sw (id::extOn, "External Input On", false));
    l.add (pot (id::noiseVol, "Noise Volume", 0.0f));
    l.add (sw (id::noiseOn, "Noise On", false));
    l.add (std::make_unique<APC> (juce::ParameterID { id::noiseColor, 1 }, "Noise Color",
                                  juce::StringArray { "White", "Pink" }, 0));

    // Modifiers
    l.add (cutoffPot (id::cutoff, "Cutoff Frequency", 6.0f));
    l.add (pot (id::emphasis, "Emphasis", 2.0f));
    l.add (pot (id::contourAmt, "Amount of Contour", 5.0f));
    l.add (sw (id::kbd1, "Keyboard Control 1", false));
    l.add (sw (id::kbd2, "Keyboard Control 2", true));
    l.add (timePot (id::fAttack, "Filter Attack", 0.0f, true, false));
    l.add (timePot (id::fDecay, "Filter Decay", 4.0f, false, false));
    l.add (pot (id::fSustain, "Filter Sustain", 5.0f));
    l.add (timePot (id::lAttack, "Loudness Attack", 0.0f, true, true));
    l.add (timePot (id::lDecay, "Loudness Decay", 4.0f, false, true));
    l.add (pot (id::lSustain, "Loudness Sustain", 10.0f));

    // Output and left-hand controller
    l.add (pot (id::volume, "Volume", 7.0f));
    l.add (sw (id::mainOn, "Main Output On", true));
    l.add (sw (id::a440, "A-440", false));
    l.add (sw (id::glideOn, "Glide On", false));
    l.add (sw (id::decayOn, "Decay", false));

    // Non-original
    l.add (sw (id::feedback, "Feedback Patch (Output -> Ext In)", false));
    l.add (std::make_unique<APC> (juce::ParameterID { id::voiceMode, 1 }, "Voice Mode",
                                  juce::StringArray { "Mono", "Poly" }, 0));
    l.add (std::make_unique<API> (juce::ParameterID { id::voices, 1 }, "Poly Voices", 2, 8, 6));
    l.add (std::make_unique<APC> (juce::ParameterID { id::quality, 1 }, "Quality",
                                  juce::StringArray { "Eco 2x", "High 4x", "Ultra 8x" }, 1));
    l.add (std::make_unique<APF> (juce::ParameterID { id::drift, 1 }, "Analog Drift",
                                  juce::NormalisableRange<float> (0.0f, 3.0f, 0.1f), 1.0f,
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));
    l.add (std::make_unique<API> (juce::ParameterID { id::unitSeed, 1 }, "Unit", 1, 999, 1));
    l.add (std::make_unique<API> (juce::ParameterID { id::bendRange, 1 }, "Bend Range", 1, 12, 5));
    // Modulation depth kept with the sound (the MOD. wheel is a performance
    // control and is not stored): the deeper of the two is used.
    l.add (pot (id::modAmount, "Modulation Amount", 0.0f));
    return l;
}

std::atomic<float>* ParameterCache::get (juce::AudioProcessorValueTreeState& s, const char* pid)
{
    auto* p = s.getRawParameterValue (pid);
    jassert (p != nullptr);
    return p;
}

void ParameterCache::attach (juce::AudioProcessorValueTreeState& s)
{
    tune = get (s, id::tune); glide = get (s, id::glide); modMix = get (s, id::modMix);
    oscMod = get (s, id::oscMod); filterMod = get (s, id::filterMod);
    osc3Control = get (s, id::osc3Control);
    for (int i = 0; i < 3; ++i)
    {
        oscRange[i] = get (s, id::oscRange[i]);
        oscWave[i] = get (s, id::oscWave[i]);
        oscFreq[i] = i > 0 ? get (s, id::oscFreq[i]) : nullptr;
        oscVol[i] = get (s, id::oscVol[i]);
        oscOn[i] = get (s, id::oscOn[i]);
    }
    extVol = get (s, id::extVol); extOn = get (s, id::extOn);
    noiseVol = get (s, id::noiseVol); noiseOn = get (s, id::noiseOn); noiseColor = get (s, id::noiseColor);
    cutoff = get (s, id::cutoff); emphasis = get (s, id::emphasis); contourAmt = get (s, id::contourAmt);
    kbd1 = get (s, id::kbd1); kbd2 = get (s, id::kbd2);
    fAttack = get (s, id::fAttack); fDecay = get (s, id::fDecay); fSustain = get (s, id::fSustain);
    lAttack = get (s, id::lAttack); lDecay = get (s, id::lDecay); lSustain = get (s, id::lSustain);
    volume = get (s, id::volume); mainOn = get (s, id::mainOn); a440 = get (s, id::a440);
    glideOn = get (s, id::glideOn); decayOn = get (s, id::decayOn); feedback = get (s, id::feedback);
    voiceModeP = get (s, id::voiceMode); voicesP = get (s, id::voices); qualityP = get (s, id::quality);
    drift = get (s, id::drift); unitSeedP = get (s, id::unitSeed); bendRange = get (s, id::bendRange);
    modAmount = get (s, id::modAmount);
}

int ParameterCache::oversampling() const noexcept
{
    const int q = int (qualityP->load());
    return q == 0 ? 2 : q == 1 ? 4 : 8;
}

void ParameterCache::read (PanelState& p) const noexcept
{
    auto pot01 = [] (const std::atomic<float>* a) { return double (a->load()) / 10.0; };
    auto on = [] (const std::atomic<float>* a) { return a->load() > 0.5f; };

    p.tune = 0.5 + double (tune->load()) / 3.0;            // +/-1.5 st -> 0..1
    p.glide = pot01 (glide);
    p.modMix = pot01 (modMix);
    p.oscModulation = on (oscMod);
    p.filterModulation = on (filterMod);
    p.osc3Control = on (osc3Control);
    for (int i = 0; i < 3; ++i)
    {
        auto& o = p.osc[size_t (i)];
        o.range = Range (int (oscRange[i]->load()));
        o.wave = Waveform (int (oscWave[i]->load()));
        o.frequency = i > 0 ? 0.5 + double (oscFreq[i]->load()) / 14.0 : 0.5; // +/-7 st -> 0..1
        o.volume = pot01 (oscVol[i]);
        o.on = on (oscOn[i]);
    }
    p.extVolume = pot01 (extVol);
    p.extOn = on (extOn);
    p.noiseVolume = pot01 (noiseVol);
    p.noiseOn = on (noiseOn);
    p.noisePink = on (noiseColor);
    p.cutoff = pot01 (cutoff);
    p.emphasis = pot01 (emphasis);
    p.contourAmount = pot01 (contourAmt);
    p.kbd1 = on (kbd1);
    p.kbd2 = on (kbd2);
    p.fAttack = pot01 (fAttack); p.fDecay = pot01 (fDecay); p.fSustain = pot01 (fSustain);
    p.lAttack = pot01 (lAttack); p.lDecay = pot01 (lDecay); p.lSustain = pot01 (lSustain);
    p.volume = pot01 (volume);
    p.mainOn = on (mainOn);
    p.a440 = on (a440);
    p.glideOn = on (glideOn);
    p.decayOn = on (decayOn);
    p.feedbackPatch = on (feedback);
    p.drift = double (drift->load());
    p.bendRange = double (bendRange->load());
    p.modAmount = pot01 (modAmount);
}

} // namespace mm::params
