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


// The service manual's checks: what section 5 of the Minimoog service manual
// (and R. J. Folkman's field service manual, 7/73) has a technician verify on
// a unit, run on the simulated units. Each test names the paragraph it follows.
// Levels in dB are read as on an AC voltmeter whose 0 dB is 0.775 V.

#include "circuit/blocks/ExpoConverter.h"
#include "circuit/blocks/FrontPanel.h"
#include "circuit/blocks/Glide.h"
#include "circuit/blocks/NoiseSource.h"
#include "engine/DriftModel.h"
#include "engine/PanelLaws.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

using namespace mm;
using Catch::Approx;

namespace
{
constexpr int lowA = 45, thirdA = 69, highA = 81; // first, third and fourth A of the keyboard

double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

/** dB re 0.775 V of an engine output's RMS (the engine's samples are volts at
    the jack times SynthEngine::outputScale). */
double dBm (double rmsSamples) { return 20.0 * std::log10 (rmsSamples / SynthEngine::outputScale / 0.775); }

/** Renders a held note and returns the output from 1 s on. `rate`: the host
    rate; a level read on a wideband meter needs more than 48 kHz (white noise
    reaches well above 24 kHz at the jack). */
std::vector<float> held (PanelState p, int note, double seconds, uint64_t unit = 1, bool play = true,
                         double rate = 48000.0)
{
    SynthEngine e;
    e.build (unit);
    e.prepare (rate, 256, 4);
    p.drift = 0.0;
    e.setPanel (p);
    std::vector<float> y (size_t (rate * (1.0 + seconds)));
    if (play) e.noteOn (note, 1.0f);
    for (size_t i = 0; i < y.size(); i += 256)
        e.process (y.data() + i, nullptr, int (std::min<size_t> (256, y.size() - i)));
    return { y.begin() + std::ptrdiff_t (rate), y.end() };
}

double rmsOf (const std::vector<float>& y)
{
    double s = 0.0;
    for (float v : y) s += double (v) * double (v);
    return std::sqrt (s / double (y.size()));
}

/** Fundamental (Hz) from rising zero crossings. */
double frequencyOf (const std::vector<float>& y)
{
    double first = -1.0, last = -1.0;
    int n = 0;
    for (size_t i = 0; i + 1 < y.size(); ++i)
        if (y[i] <= 0.0f && y[i + 1] > 0.0f)
        {
            const double t = double (i) + y[i] / (y[i] - y[i + 1]);
            if (n == 0) first = t;
            last = t;
            ++n;
        }
    return n > 1 ? (n - 1) * 48000.0 / (last - first) : 0.0;
}

/** Pitch swing (octaves, peak to peak) from the intervals between rising zero
    crossings: 2nd to 98th percentile. */
double pitchSwing (const std::vector<float>& y)
{
    std::vector<double> f;
    double last = -1.0;
    for (size_t i = 0; i + 1 < y.size(); ++i)
        if (y[i] <= 0.0f && y[i + 1] > 0.0f)
        {
            const double t = double (i) + y[i] / (y[i] - y[i + 1]);
            if (last >= 0.0) f.push_back (48000.0 / (t - last));
            last = t;
        }
    std::sort (f.begin(), f.end());
    return std::log2 (f[size_t (0.98 * double (f.size() - 1))] / f[size_t (0.02 * double (f.size() - 1))]);
}

/** The panel as section 5 leaves it for the level checks: one source, the
    filter wide open, the loudness contour sustained at 10, VOLUME at 10. */
PanelState levelPanel()
{
    PanelState p;
    for (auto& o : p.osc) { o.on = false; o.wave = Waveform::Triangle; o.volume = 1.0; }
    p.cutoff = 1.0;
    p.emphasis = 0.0;
    p.contourAmount = 0.0;
    p.lAttack = 0.0;
    p.lDecay = 0.0;
    p.lSustain = 1.0;
    p.volume = 1.0;
    return p;
}

/** The filter singing alone: sources off, EMPHASIS at 10. */
PanelState singingFilter()
{
    PanelState p = levelPanel();
    p.emphasis = 1.0;
    return p;
}
} // namespace

TEST_CASE ("5.31-5.33 Oscillator range and scale: 3520 Hz on high A, 440 Hz on low A at 2'", "[manual][vco]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull, 4ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        for (auto& o : p.osc) o.range = Range::Ft2;
        for (int osc = 0; osc < 3; ++osc)
        {
            INFO ("unit " << unit << " oscillator " << osc + 1);
            CHECK (std::abs (cents (v.oscFrequencyFor (osc, p, highA), 3520.0)) < 2.0);
            CHECK (std::abs (cents (v.oscFrequencyFor (osc, p, lowA), 440.0)) < 2.0);
            // In between, what the fixed high-frequency compensation (R42) and the
            // key string leave: a few cents.
            CHECK (std::abs (cents (v.oscFrequencyFor (osc, p, thirdA), 1760.0)) < 6.0);
        }
    }
}

TEST_CASE ("Octave adjust (Folkman; 5.34): oscillator 1 at 8' is two octaves below 2'; LO is 4.75 octaves below 32'", "[manual][vco]")
{
    CHECK (FrontPanel::rangeOctaves[0] == Approx (-6.75));
    for (uint64_t unit : { 1ull, 2ull, 3ull, 4ull })
    {
        Voice v;
        v.build (unit);
        INFO ("unit " << unit);
        // Folkman: "Switch RANGE to 8 ft. Depress same key and adjust Octave
        // Range trimpot ... for zero beats" (the service manual's 5.34 uses 16')
        CHECK ((v.rangeOctaves (0, Range::Ft2) - v.rangeOctaves (0, Range::Ft8)) * 1200.0 == Approx (2400.0).margin (1.0));
        // The other steps and oscillators: the matched resistors' mismatch (the
        // RNG inputs to 0.1 %: up to about 2.5 cents an octave away from 2')
        for (int osc = 0; osc < 3; ++osc)
        {
            INFO ("oscillator " << osc + 1 << ": 8' " << v.rangeOctaves (osc, Range::Ft8) * 1200.0 << " cents, 16' "
                  << v.rangeOctaves (osc, Range::Ft16) * 1200.0 + 1200.0 << ", 32' "
                  << v.rangeOctaves (osc, Range::Ft32) * 1200.0 + 2400.0);
            CHECK (v.rangeOctaves (osc, Range::Ft8) * 1200.0 == Approx (0.0).margin (5.0));
            CHECK (v.rangeOctaves (osc, Range::Ft16) * 1200.0 == Approx (-1200.0).margin (7.5));
            CHECK (v.rangeOctaves (osc, Range::Ft32) * 1200.0 == Approx (-2400.0).margin (10.0));
            // R29 4.75K 1 % against the 1K steps
            CHECK (v.rangeOctaves (osc, Range::Ft32) - v.rangeOctaves (osc, Range::Lo) == Approx (4.75).epsilon (0.012));
        }
    }
}

TEST_CASE ("5.35 Range of the TUNE and FREQUENCY controls", "[manual][vco]")
{
    // "TUNE control 4-6 Semitones, OSCILLATOR-2 FREQUENCY adjust 14-17
    // Semitones, OSCILLATOR-3 the same", counter-clockwise to clockwise. The
    // drawn networks give 5.4 and 16.8 with nominal pots.
    CHECK (12.0 * (FrontPanel::tuneOctaves (1.0) - FrontPanel::tuneOctaves (0.0)) == Approx (5.42).margin (0.05));
    CHECK (12.0 * (FrontPanel::frequencyOctaves (1.0) - FrontPanel::frequencyOctaves (0.0)) == Approx (16.8).margin (0.1));
    for (uint64_t unit : { 1ull, 2ull, 3ull })
    {
        Voice v;
        v.build (unit);
        PanelState lo, hi;
        lo.tune = 0.0; hi.tune = 1.0;
        const double tune = 12.0 * std::log2 (v.oscFrequencyFor (0, hi, 60) / v.oscFrequencyFor (0, lo, 60));
        INFO ("unit " << unit << " TUNE " << tune << " semitones");
        CHECK (tune > 4.0);
        CHECK (tune < 6.5);
        for (int osc = 1; osc < 3; ++osc)
        {
            PanelState a, b;
            a.osc[size_t (osc)].frequency = 0.0;
            b.osc[size_t (osc)].frequency = 1.0;
            const double span = 12.0 * std::log2 (v.oscFrequencyFor (osc, b, 60) / v.oscFrequencyFor (osc, a, 60));
            INFO ("oscillator " << osc + 1 << " FREQUENCY " << span << " semitones");
            CHECK (span > 14.0);
            CHECK (span < 18.5); // 20 % pots: a little over the manual's 17 on some units
            // linear pot: the centre is the middle
            PanelState c;
            CHECK (12.0 * std::log2 (v.oscFrequencyFor (osc, c, 60) / v.oscFrequencyFor (osc, a, 60)) == Approx (0.5 * span).margin (0.01));
        }
    }
}

TEST_CASE ("5.36 Oscillator 3 wide range: clicks two to five seconds apart, LO overlaps 32'", "[manual][vco]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        p.osc3Control = false;
        p.osc[2].wave = Waveform::Sawtooth;
        p.osc[2].range = Range::Lo;
        p.osc[2].frequency = 0.0;
        // "Assure that the keyboard has no effect on OSCILLATOR-3 pitch"
        CHECK (v.oscFrequencyFor (2, p, 41) == v.oscFrequencyFor (2, p, 84));
        // ... nor the PITCH wheel and TUNE (2.18; Fig. 9-3)
        PanelState bent = p;
        bent.pitchWheel = 1.0;
        bent.tune = 1.0;
        CHECK (v.oscFrequencyFor (2, bent, 41) == v.oscFrequencyFor (2, p, 41));
        const double period = 1.0 / v.oscFrequencyFor (2, p, 60);
        INFO ("unit " << unit << ": a click every " << period << " s");
        CHECK (period > 2.0);
        CHECK (period < 5.0);
        // "the high end of the LO range overlaps the low end of the 32' range"
        PanelState top = p, bottom = p;
        top.osc[2].frequency = 1.0;
        bottom.osc[2].range = Range::Ft32;
        CHECK (v.oscFrequencyFor (2, top, 60) > v.oscFrequencyFor (2, bottom, 60));
        // The FREQUENCY knob is 4.4 times wider: "a frequency sweep of 6 octaves
        // rather than one octave" (owner's manual)
        const double wide = std::log2 (v.oscFrequencyFor (2, top, 60) / v.oscFrequencyFor (2, p, 60));
        CHECK (wide > 5.2);
        CHECK (wide < 7.0);
    }
}

TEST_CASE ("5.37 Modulation of the oscillators: 13 to 23 semitones with the MOD wheel fully up", "[manual][modulation]")
{
    // Oscillator 3's lowest square wave on the modulation line, wheel fully up.
    PanelState p = levelPanel();
    p.osc[0].on = true;
    p.osc[0].range = Range::Ft2;
    p.osc[2].range = Range::Lo;
    p.osc[2].wave = Waveform::Square;
    p.osc3Control = false;
    p.modMix = 0.0;
    p.oscModulation = true;
    p.modWheel = 1.0;
    const double semis = 12.0 * pitchSwing (held (p, 57, 6.0));
    INFO ("oscillator 1 swings " << semis << " semitones");
    CHECK (semis > 13.0);
    CHECK (semis < 23.0);
    // "Set MOD control wheel all the way down. No modulation should be evident."
    p.modWheel = 0.0;
    CHECK (12.0 * pitchSwing (held (p, 57, 3.0)) < 0.05);
}

TEST_CASE ("5.19 Amount of modulation: the filter goes from 440 Hz to at least 2.4 kHz", "[manual][modulation]")
{
    PanelState p = singingFilter();
    p.cutoff = 0.5;
    p.osc[2].range = Range::Lo;
    p.osc[2].wave = Waveform::Square;
    p.osc3Control = false;
    p.modMix = 0.0;
    p.filterModulation = true;
    p.modWheel = 1.0;
    const double octaves = pitchSwing (held (p, 57, 6.0));
    INFO ("the filter swings " << octaves << " octaves");
    CHECK (octaves > std::log2 (2400.0 / 440.0)); // "a minimum of 2.4kHz"
}

TEST_CASE ("5.12 Emphasis calibration: regeneration starts between 7 and 8", "[manual][filter]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull, 4ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        p.cutoff = laws::cutoffKnobAt (-1.0);
        const double cv = v.filterCv (p, 41, 0.0);
        INFO ("unit " << unit << ", REGEN CAL at " << v.filter().params().regenCal);
        CHECK_FALSE (v.filterRegenerates (cv, 0.70));
        CHECK (v.filterRegenerates (cv, 0.80));
        CHECK (v.filter().params().regenCal > 0.05);
        CHECK (v.filter().params().regenCal < 0.95);
    }
}

TEST_CASE ("5.13 Filter cutoff: 440 Hz at -1, at least 16 kHz fully clockwise, still singing under 300 Hz", "[manual][filter]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull, 4ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        p.cutoff = laws::cutoffKnobAt (-1.0);
        INFO ("unit " << unit);
        CHECK (std::abs (cents (v.filterSelfOscillationHz (v.filterCv (p, 41, 0.0)), 440.0)) < 5.0);
        p.cutoff = 1.0;
        CHECK (v.filterCutoffHz (v.filterCv (p, 41, 0.0)) > 16000.0);
        p.cutoff = laws::cutoffKnobAt (-2.0);
        const double low = v.filterSelfOscillationHz (v.filterCv (p, 41, 0.0));
        CHECK (low > 100.0);
        CHECK (low < 300.0);
    }
}

TEST_CASE ("5.14-5.15 Filter scale: an octave a key octave with KEYBOARD CONTROL 1 and 2; 1/3 with 1 alone", "[manual][filter]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        p.kbd1 = p.kbd2 = true;
        // "Depress the third A ... adjust the CUTOFF FREQUENCY control for 1760Hz"
        p.cutoff = 0.5;
        for (int it = 0; it < 4; ++it)
            p.cutoff += std::log2 (1760.0 / v.filterSelfOscillationHz (v.filterCv (p, thirdA, 0.0))) / 10.0;
        INFO ("unit " << unit);
        CHECK (std::abs (cents (v.filterSelfOscillationHz (v.filterCv (p, thirdA, 0.0)), 1760.0)) < 25.0);
        const double fThird = v.filterSelfOscillationHz (v.filterCv (p, thirdA, 0.0));
        // "Depress low A and adjust the Filter Scale ... zero beat"
        CHECK (std::abs (cents (v.filterSelfOscillationHz (v.filterCv (p, lowA, 0.0)), fThird / 4.0)) < 10.0);
        // "until filter scale covers three octaves"
        CHECK (std::abs (cents (v.filterSelfOscillationHz (v.filterCv (p, highA, 0.0)), fThird * 2.0)) < 40.0);

        // 5.15: KEYBOARD CONTROL 2 off, low A tuned to 440 Hz: "Depress high A
        // key; frequency should be 880 +/- 50Hz"
        p.kbd2 = false;
        for (int it = 0; it < 4; ++it)
            p.cutoff += std::log2 (440.0 / v.filterSelfOscillationHz (v.filterCv (p, lowA, 0.0))) / 10.0;
        const double third = v.filterSelfOscillationHz (v.filterCv (p, highA, 0.0))
                           / v.filterSelfOscillationHz (v.filterCv (p, lowA, 0.0)) * 440.0;
        CHECK (third == Approx (880.0).margin (50.0));
    }
}

TEST_CASE ("5.17-5.18 Amount of contour drives the filter to its top; two octaves in about two divisions", "[manual][filter]")
{
    for (uint64_t unit : { 1ull, 2ull, 3ull })
    {
        Voice v;
        v.build (unit);
        PanelState p;
        p.cutoff = laws::cutoffKnobAt (-1.0);
        // 5.18: "until the frequency rises from 440 to 1760Hz (two octaves up).
        // A rotation of approximately 2 divisions should be required."
        PanelState up = p;
        up.cutoff = laws::cutoffKnobAt (1.0);
        const double octaves = std::log2 (v.filterSelfOscillationHz (v.filterCv (up, 41, 0.0))
                                        / v.filterSelfOscillationHz (v.filterCv (p, 41, 0.0)));
        INFO ("unit " << unit << ": two divisions give " << octaves << " octaves");
        CHECK (octaves == Approx (2.0).margin (0.25));
        // 5.17: "Rotate AMOUNT OF CONTOUR control fully clockwise. Pitch should
        // rise from 440Hz to 35kHz +/- 10kHz": the filter contour held at its
        // sustain (S-trigger in), which takes the converter to its limit.
        p.contourAmount = 1.0;
        const auto fc = ContourParams::filter();
        const double sustained = std::fmin (fc.sustainCap (1.0), fc.peakOutput() + fc.vOutDrop) - fc.vOutDrop;
        const double top = v.filterCutoffHz (v.filterCv (p, 41, sustained));
        INFO ("contour at " << sustained << " V: " << top << " Hz");
        CHECK (top > 25000.0);
        CHECK (top < 45000.0);
    }
    CHECK (laws::cutoffHz (laws::cutoffKnobAt (-1.0)) == Approx (440.0));
    CHECK (laws::cutoffHz (1.0) == Approx (440.0 * 64.0));
}

TEST_CASE ("5.8, 5.27, 5.7 Levels at the main output: triangle +1 dB, noise -5 dB, A-440 -8 dB", "[manual][levels]")
{
    // 5.8 Signal flow: "turn on OSCILLATOR-1 and check for a triangular output
    // of 1 +/- 3dB"
    PanelState p = levelPanel();
    p.osc[0].on = true;
    p.osc[0].range = Range::Ft8;
    const double triangle = dBm (rmsOf (held (p, 69, 1.0)));
    INFO ("triangle " << triangle << " dB");
    CHECK (triangle == Approx (1.0).margin (3.0));

    // 5.27 Audio noise generator: "Noise level should be -5 +/- 3dB ... so that
    // both white and pink fall within specification"
    p.osc[0].on = false;
    p.noiseOn = true;
    p.noiseVolume = 1.0;
    // Read as the technician's AC voltmeter does, over the jack's whole band.
    const double white = dBm (rmsOf (held (p, 69, 2.0, 1, true, 96000.0)));
    p.noisePink = true;
    const double pink = dBm (rmsOf (held (p, 69, 2.0, 1, true, 96000.0)));
    INFO ("white " << white << " dB, pink " << pink << " dB");
    CHECK (white == Approx (-5.0).margin (3.0));
    CHECK (pink == Approx (-5.0).margin (3.0));
    CHECK (0.5 * (white + pink) == Approx (-5.0).margin (1.0)); // the Noise Level trim centres the two

    // 5.7 A-440: "Check to see that output is -8 +/- 2dB"; it sounds with no key down.
    p.noiseOn = false;
    p.a440 = true;
    const auto ref = held (p, 69, 1.0, 1, false);
    INFO ("A-440 " << dBm (rmsOf (ref)) << " dB at " << frequencyOf (ref) << " Hz");
    CHECK (dBm (rmsOf (ref)) == Approx (-8.0).margin (2.0));
    // ngspice: the drawn Wien bridge into the drawn VCA through R40 10K
    // (tools/spice/a440.cir, a440_level.cir): -8.3 dB
    CHECK (dBm (rmsOf (ref)) == Approx (-8.3).margin (1.0));
    CHECK (frequencyOf (ref) == Approx (440.0).margin (1.0)); // "440 +/- 1Hz"

    // 5.24 Noise: "Turn off OSCILLATOR-1. Noise level at HIGH MAIN OUTPUT jack
    // with volume fully clockwise and S-trigger inserted should be [-]65dB maximum."
    p.a440 = false;
    const double floor = dBm (rmsOf (held (p, 69, 1.0)));
    INFO ("residual noise " << floor << " dB");
    CHECK (floor < -65.0);
}

TEST_CASE ("5.25 Contour generator balance: the contour leaves less than -55 dB on the output", "[manual][vca]")
{
    // What 1ST VCA BAL nulls: the first pair's imbalance, a step on the output
    // proportional to the tail current each time the contour opens.
    for (uint64_t unit : { 1ull, 2ull, 3ull, 4ull })
    {
        Voice v;
        v.build (unit);
        Vca vca = v.amplifier();
        vca.prepare (48000.0);
        double peak = 0.0;
        for (int i = 0; i < 4800; ++i)
            peak = std::fmax (peak, std::abs (vca.process (0.0, i < 480 ? -0.5 : 4.5)));
        INFO ("unit " << unit << ": " << 20.0 * std::log10 (peak / 0.775 + 1e-12) << " dB");
        CHECK (20.0 * std::log10 (peak / 0.775 + 1e-12) < -55.0);
    }
}

TEST_CASE ("2.19 Keyboard and glide: a volt an octave, 1 ms to 1 s an octave, held when the key is released", "[manual][glide]")
{
    // Specifications: "RATE OF KEYBOARD GLIDE: Continuously variable from
    // 1 millisecond to 1 second/octave."
    const double fs = 48000.0;
    auto octaveTime = [fs] (double pot, bool on, double from, double to) {
        Glide g;
        g.setParams ({});
        g.prepare (fs);
        g.setTime (pot);
        g.setEnabled (on);
        g.reset (from);
        // time over the middle 90 % of the interval
        const double a = from + 0.05 * (to - from), b = from + 0.95 * (to - from);
        int start = -1;
        for (int i = 0; i < int (fs * 20.0); ++i)
        {
            const double y = g.process (to, true);
            if (start < 0 && (to > from ? y >= a : y <= a)) start = i;
            if (to > from ? y >= b : y <= b) return double (i - start) / fs / 0.9 / std::abs (to - from);
        }
        return 1.0e9;
    };
    const double upMax = octaveTime (1.0, true, 1.0, 2.0), downMax = octaveTime (1.0, true, 2.0, 1.0);
    INFO ("GLIDE 10: " << upMax << " s an octave up, " << downMax << " down");
    CHECK (upMax > 0.4);
    CHECK (upMax < 1.2);
    CHECK (downMax > 0.4);
    CHECK (downMax < 1.2);
    CHECK (upMax < downMax); // Q24's rail is further away than its -4.4 V rest
    // ngspice on the drawn circuit, the pot at its full 5M (tools/spice/keyboard.cir)
    CHECK (upMax == Approx (0.582).epsilon (0.03));
    CHECK (downMax == Approx (1.074).epsilon (0.03));
    CHECK (octaveTime (0.0, true, 1.0, 2.0) < 1.0e-3);
    CHECK (octaveTime (1.0, false, 1.0, 2.0) < 1.0e-3); // GLIDE switch off: "an instant pitch change"
    // monotonic in the knob
    for (int i = 0; i < 10; ++i)
        CHECK (octaveTime (0.1 * (i + 1), true, 1.0, 2.0) > octaveTime (0.1 * i, true, 1.0, 2.0));

    // "When the key is released, Q13 is turned off and the pitch voltage is
    // held": the glide stops where it is.
    Glide g;
    g.setParams ({});
    g.prepare (fs);
    g.setTime (1.0);
    g.setEnabled (true);
    g.reset (1.0);
    for (int i = 0; i < int (fs * 0.2); ++i) g.process (2.0, true);
    const double atRelease = g.value();
    CHECK (atRelease > 1.05);
    CHECK (atRelease < 1.95);
    for (int i = 0; i < int (fs * 1.0); ++i) g.process (2.0, false);
    // Folkman: more than a semitone a minute of drift is a fault; a sound unit
    // sags by a few cents a minute.
    const double centsPerMinute = (g.value() - atRelease) * 1200.0 * 60.0;
    INFO ("hold drift " << centsPerMinute << " cents a minute");
    CHECK (centsPerMinute != 0.0);
    CHECK (std::abs (centsPerMinute) < 20.0);

    // The keys: 43 resistors of 10 ohm 1 %; the two A's the oscillators are
    // scaled on are exact.
    Voice v;
    v.build (3);
    CHECK (v.keyVolts (highA) - v.keyVolts (lowA) == Approx (3.0).margin (1e-9));
    for (int n = 42; n <= 84; ++n)
        CHECK ((v.keyVolts (n) - v.keyVolts (n - 1)) * 1200.0 == Approx (100.0).margin (1.5));
}

TEST_CASE ("KEYBOARD CONTROL: the filter follows the keys at 1/3, 2/3 and 1 octave an octave", "[manual][filter]")
{
    // The filter singing alone, CUTOFF at its centre, C3 then C4 through the
    // whole instrument: KBD 1 (R53 300K), KBD 2 (R54 150K) and both.
    struct Case { bool kbd1, kbd2; double octaves; };
    for (const auto& c : { Case { true, false, 1.0 / 3.0 }, Case { false, true, 2.0 / 3.0 }, Case { true, true, 1.0 } })
    {
        INFO ("KBD 1 " << c.kbd1 << ", KBD 2 " << c.kbd2);
        PanelState p = singingFilter();
        p.cutoff = 0.5;
        p.kbd1 = c.kbd1;
        p.kbd2 = c.kbd2;
        const double low = frequencyOf (held (p, 48, 1.0)), high = frequencyOf (held (p, 60, 1.0));
        CHECK (std::log2 (high / low) == Approx (c.octaves).margin (0.03));
    }
}

TEST_CASE ("Modulation line: depth and MOD. wheel law from the circuit", "[manual][modulation]")
{
    // Oscillator 3's triangle on LO through MODULATION MIX into the line (Fig.
    // 9-8, 9-12). Fully forward the wheel is 1.2K against R57 1K: the
    // oscillators swing +-0.74 octave. Half way it is 0.41K: about half.
    PanelState base = levelPanel();
    base.osc[2].range = Range::Lo;
    base.osc[2].wave = Waveform::Triangle;
    base.osc3Control = false;
    base.modMix = 0.0;

    PanelState osc = base;
    osc.osc[0].on = true;
    osc.oscModulation = true;
    osc.modWheel = 1.0;
    const double full = pitchSwing (held (osc, 57, 4.0));
    osc.modWheel = 0.5;
    const double half = pitchSwing (held (osc, 57, 4.0));

    PanelState filter = base;
    filter.cutoff = 0.5;
    filter.emphasis = 1.0;
    filter.filterModulation = true;
    filter.modWheel = 0.5;
    const double filterHalf = pitchSwing (held (filter, 57, 4.0));

    INFO ("oscillators: " << full << " oct at full, " << half << " at half; filter " << filterHalf << " at half");
    CHECK (full == Approx (2.0 * 0.74).epsilon (0.1));
    CHECK (half / full == Approx (0.54).epsilon (0.1));
    // FILTER MODULATION: 100K / 33K as deep as the oscillators' (the loads differ a little)
    CHECK (filterHalf / half == Approx (100.0 / 33.0).epsilon (0.12));
}

TEST_CASE ("Modulation polarity: oscillator 3's rising sawtooth lowers the pitch", "[manual][modulation]")
{
    // The modulation amplifier inverts (Fig. 9-8) and the line enters the
    // oscillators' summer as the keyboard does (Fig. 9-3): during oscillator
    // 3's rising ramp the pitch falls, and it jumps up at each reset.
    PanelState p = levelPanel();
    p.osc[0].on = true;
    p.osc[2].range = Range::Lo;
    p.osc[2].wave = Waveform::Sawtooth;
    p.osc3Control = false;
    p.modMix = 0.0;
    p.modWheel = 1.0;
    p.oscModulation = true;
    const auto y = held (p, 57, 4.0);
    std::vector<double> f;
    double last = -1.0;
    for (size_t i = 0; i + 1 < y.size(); ++i)
        if (y[i] <= 0.0f && y[i + 1] > 0.0f)
        {
            const double t = double (i) + y[i] / (y[i] - y[i + 1]);
            if (last >= 0.0) f.push_back (48000.0 / (t - last));
            last = t;
        }
    // Steps between consecutive periods, in octaves: the large ones are the
    // ramp's resets; they must go up, and the ramp in between must go down.
    double biggestUp = 0.0, biggestDown = 0.0;
    int falling = 0, rising = 0;
    for (size_t i = 1; i < f.size(); ++i)
    {
        const double d = std::log2 (f[i] / f[i - 1]);
        biggestUp = std::fmax (biggestUp, d);
        biggestDown = std::fmin (biggestDown, d);
        if (std::abs (d) < 0.05) (d < 0.0 ? falling : rising)++;
    }
    INFO ("biggest step up " << biggestUp << " oct, down " << biggestDown << " oct; small steps falling "
          << falling << ", rising " << rising);
    CHECK (biggestUp > 0.5);
    CHECK (biggestUp > 5.0 * -biggestDown);
    CHECK (falling > 3 * rising);
}

TEST_CASE ("Pink and red noise follow board 3's drawn networks", "[manual][noise]")
{
    // R16 10K into C3 .12 uF + R8 3.3K, C2 .033 uF + R13 220 and Q3's input
    // (Fig. 9-8): the pink output against the white, in third-octave bands,
    // against the network's own transfer function, with Q3's input resistance
    // and gain as ngspice gives them on the drawn circuit (tools/spice/noise.cir).
    const double fs = 96000.0;
    auto network = [] (double f) {
        // |gain * Zp / (R16 + Zp)|^2 with Zp the three branches in parallel
        const double w = 2.0 * M_PI * f;
        struct C { double re, im; };
        auto inv = [] (C z) { const double d = z.re * z.re + z.im * z.im; return C { z.re / d, -z.im / d }; };
        const C y1 = inv ({ 3.3e3, -1.0 / (w * 0.12e-6) }), y2 = inv ({ 220.0, -1.0 / (w * 0.033e-6) }),
                y3 = inv ({ 26.9e3, -1.0 / (w * 2.5e-6) });
        const C zp = inv ({ y1.re + y2.re + y3.re, y1.im + y2.im + y3.im });
        const C den { 10.0e3 + zp.re, zp.im };
        const double num = zp.re * zp.re + zp.im * zp.im, d = den.re * den.re + den.im * den.im;
        return 16.9 * 16.9 * num / d;
    };
    NoiseSource n;
    n.prepare (fs, 5);
    n.setWhiteLevel (1.0);
    const int count = int (fs * 20.0);
    std::vector<double> w, pk, rd;
    w.resize (size_t (count));
    pk.resize (size_t (count));
    rd.resize (size_t (count));
    for (int i = 0; i < count; ++i)
        n.process (w[size_t (i)], pk[size_t (i)], rd[size_t (i)]);
    // Band power by a two-pole resonator at each frequency
    auto band = [fs] (const std::vector<double>& x, double f, double q = 4.0) {
        const double w0 = 2.0 * M_PI * f / fs, alpha = std::sin (w0) / (2.0 * q);
        const double b0 = alpha, a0 = 1.0 + alpha, a1 = -2.0 * std::cos (w0), a2 = 1.0 - alpha;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, s = 0;
        for (double v : x)
        {
            const double y = (b0 * v - b0 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1; x1 = v; y2 = y1; y1 = y;
            s += y * y;
        }
        return s;
    };
    for (double f : { 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const double dB = 10.0 * std::log10 (band (pk, f) / band (w, f));
        INFO (f << " Hz: pink / white " << dB << " dB, network " << 10.0 * std::log10 (network (f)));
        CHECK (dB == Approx (10.0 * std::log10 (network (f))).margin (1.0));
    }
    // Red: R12 10K into C7 .15 uF, then the common emitter stage Q6 with C22
    // .15 uF across its load: louder than pink in the bass, two poles above.
    // The red output against the pink, against ngspice on the drawn circuit;
    // narrow bands, the red falling too steeply for the wide ones.
    constexpr std::array<std::pair<double, double>, 5> redBench { { { 20.0, 4.8 }, { 50.0, 3.6 }, { 100.0, 0.6 },
                                                                    { 200.0, -6.0 }, { 500.0, -19.5 } } };
    for (const auto& [f, bench] : redBench)
    {
        const double dB = 10.0 * std::log10 (band (rd, f, 40.0) / band (pk, f, 40.0));
        INFO (f << " Hz: red / pink " << dB << " dB, ngspice " << bench);
        CHECK (dB == Approx (bench).margin (1.0));
    }
}

TEST_CASE ("Specifications: short term oscillator stability better than 0.25 %", "[manual][vco]")
{
    // DRIFT at 1 (a typical instrument): the expo converter's current, hence the
    // pitch, over a minute of the boards' temperature wandering.
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        DriftModel drift;
        const double rate = 100.0; // control updates a second
        drift.prepare (rate, seed);
        drift.setAmount (1.0);
        ExpoConverter ex;
        Rng rng (seed);
        ex.setParams (ExpoConverterParams::drawn (rng, {}));
        double lo = 1e9, hi = -1e9;
        for (int i = 0; i < int (60.0 * rate); ++i)
        {
            drift.tick();
            const double t = drift.temperature (DriftModel::Vco1);
            ex.setTemperatures (t, t + drift.pairDifference (0), drift.temperature (DriftModel::Tempco1));
            const double octaves = std::log2 (ex.current (0.0));
            lo = std::fmin (lo, octaves);
            hi = std::fmax (hi, octaves);
        }
        INFO ("seed " << seed << ": " << (hi - lo) * 1200.0 << " cents over a minute");
        CHECK (std::exp2 (hi - lo) - 1.0 < 0.0025);
        CHECK (hi - lo > 0.0); // it does drift
    }
}

TEST_CASE ("External input: the interpolator passes the audio band and rejects its images", "[engine]")
{
    for (int factor : { 2, 4, 8 })
    {
        Interpolator up;
        up.prepare (factor);
        const double fs = 48000.0, f0 = 5000.0;
        std::vector<double> y;
        std::array<double, 8> out {};
        for (int n = 0; n < 4800; ++n)
        {
            up.process (std::sin (2.0 * M_PI * f0 * n / fs), out.data());
            for (int k = 0; k < factor; ++k) y.push_back (out[size_t (k)]);
        }
        // Level of a frequency in the second half of the output (Goertzel-like correlation)
        auto level = [&y, fs, factor] (double f) {
            double re = 0.0, im = 0.0;
            const size_t start = y.size() / 2;
            for (size_t i = start; i < y.size(); ++i)
            {
                const double ph = 2.0 * M_PI * f * double (i) / (fs * factor);
                re += y[i] * std::cos (ph);
                im += y[i] * std::sin (ph);
            }
            return 2.0 * std::sqrt (re * re + im * im) / double (y.size() - start);
        };
        INFO ("factor " << factor << ": passband " << level (f0) << ", first image " << level (fs - f0));
        CHECK (level (f0) == Approx (1.0).margin (0.01));
        CHECK (level (fs - f0) < 1.0e-4); // -80 dB
    }
}

TEST_CASE ("Folkman's Visualign: the OVERLOAD lamp blinks at the beat between oscillator 1 and the A-440", "[manual][levels]")
{
    // "Turn all MIXER switches off. Set MIXER VOLUMES at 1. Turn on MAIN
    // OUTPUT switch and set OUTPUT VOLUME to 8. ... Connect a short patch cord
    // from the HIGH MAIN OUTPUT jack to EXTERNAL SIGNAL INPUT jack. Turn on
    // A-440 and rotate EXTERNAL INPUT VOLUME control slowly clockwise until the
    // OVERLOAD light is just visible. Turn OSCILLATOR 1 Mixer switch on.
    // Depress low "A" key and hold, OVERLOAD light should be blinking."
    // The A-440 has to reach the jack, hence the feedback, as the instrument's
    // own signal does.
    PanelState p = levelPanel();
    p.drift = 0.0;
    for (auto& o : p.osc) o.volume = 0.1;
    p.osc[0].range = Range::Ft2;
    p.volume = 0.8;
    p.a440 = true;
    p.feedbackPatch = true;
    constexpr double rate = 48000.0;
    constexpr int block = 240; // 5 ms
    SynthEngine e;
    e.build (1);
    e.prepare (rate, block, 4);
    std::vector<float> y (block);
    auto lampOver = [&] (double seconds, std::vector<double>* trace = nullptr) {
        double sum = 0.0;
        const int n = int (seconds * rate / block);
        for (int i = 0; i < n; ++i)
        {
            e.process (y.data(), nullptr, block);
            sum += e.overloadLamp();
            if (trace) trace->push_back (e.overloadLamp());
        }
        return sum / n;
    };
    // EXTERNAL INPUT VOLUME up until the lamp is just visible on the A-440 alone
    double extVolume = 0.0, alone = 0.0;
    for (; extVolume <= 1.0; extVolume += 0.02)
    {
        p.extVolume = extVolume;
        e.setPanel (p);
        alone = lampOver (0.3);
        if (alone > 0.05) break;
    }
    INFO ("EXTERNAL INPUT VOLUME " << extVolume << ", lamp " << alone << " on the A-440 alone");
    REQUIRE (extVolume <= 1.0);
    // Oscillator 1 on, low A, two hertz above the A-440 (TUNE)
    p.osc[0].on = true;
    p.tune = 0.5 + std::log2 (442.0 / 440.0) / e.voice (0).tuneRange();
    e.setPanel (p);
    e.noteOn (lowA, 1.0f);
    lampOver (1.0);
    std::vector<double> trace;
    lampOver (4.0, &trace);
    const double lo = *std::min_element (trace.begin(), trace.end()), hi = *std::max_element (trace.begin(), trace.end());
    // blinks: rising crossings of the middle of the lamp's swing
    int blinks = 0;
    const double mid = 0.5 * (lo + hi);
    for (size_t i = 1; i < trace.size(); ++i)
        if (trace[i - 1] < mid && trace[i] >= mid) ++blinks;
    INFO ("lamp " << lo << " to " << hi << ", " << blinks << " blinks in 4 s");
    CHECK (hi > 2.0 * lo + 0.05);
    CHECK (blinks >= 6);  // 2 Hz, give or take the unit's tuning
    CHECK (blinks <= 10);
}
