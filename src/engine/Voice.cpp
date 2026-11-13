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

#include "Voice.h"
#include "PanelLaws.h"
#include "../circuit/blocks/FrontPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <utility>

namespace mm
{

namespace
{
// Control-voltage conventions (1 V/oct everywhere):
//   oscillator CV 0 V   = A4 (440 Hz) on the 8' range
//   keyboard volts      = 0 V on the lowest key, F (MIDI 41): the key string's
//                         grounded end (Fig. 9-7)
//   filter CV 0         = the CUTOFF knob at its centre, nothing else applied
constexpr int lowestKey = 41, highestKey = 84;           // the 44 keys, F to C
constexpr double keyboardZero = (lowestKey - 69) / 12.0; // the lowest key's CV against A4's
// The tuning procedures' "low A" and "high A" (first and fourth A of the
// keyboard): 440 and 3520 Hz with RANGE at 2'.
constexpr int lowA = 45, highA = 81;

// OSC. 3 CONTROL off: SW2 takes the keyboard, pitch wheel, TUNE, modulation and
// external inputs off oscillator 3's summer, and with them the range trim's
// current that balanced their standing currents (Fig. 9-3). Where the
// oscillator then sits depends on those trims; the service manual's check
// (5.36: RANGE LO, FREQUENCY fully counter-clockwise, "audible clicks which
// should occur between two to five seconds apart") puts it 1.1 to 2.4 octaves
// above its pitch on the lowest key. The middle of that window.
constexpr double osc3FreeOctaves = 1.7;

// The modulation line (board 3 circuit 15, Fig. 9-8; left hand controller,
// Fig. 9-12). MODULATION MIX drives an inverting amplifier (ModulationMix).
// The line has the keyboard's sense (the MOD inputs share the oscillators'
// summer with it), so a rising oscillator 3 lowers the pitch and the cutoff.
// The output reaches the line through R57 1K; the MODULATION wheel R1402 is a
// rheostat from the line to ground, 0 at the back and 1.2K fully forward. The
// oscillators take the line through their 51.1K MOD inputs (as the keyboard's:
// 1 octave a volt), the filter's control node through R52 33K.
constexpr double rModOut = 1.0e3;        // R57
constexpr double rWheelFull = 1.2e3;     // R1402 "1.2K WHEN FULLY FORWARD" (Fig. 9-12)
constexpr double rOscModIn = 51.1e3;     // R32 / R72 / R136
constexpr double rFilterModLoad = 33.0e3; // R52

/** The MODULATION wheel's resistance (R1402, 50K audio, "90 degrees rotation
    used from counterclockwise stop": the first 30 % of its track): 1.2K fully
    forward as drawn, with the audio taper's shape over that travel. */
double modWheelOhms (double pos)
{
    const double w = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
    return rWheelFull * taper::audio (0.3 * w) / taper::audio (0.3);
}

constexpr double calibrationRate = 96000.0; // sample rate of the bench renders below

/** Lets a copy of the filter sing with no input and returns its frequency
    (Hz), 0 if it does not oscillate. */
double selfOscillationHz (LadderFilter f, double amps)
{
    auto params = f.params();
    params.noiseRms = 0.0;
    f.setParams (params);
    f.prepare (calibrationRate, 1);
    f.setTemperature (298.15);
    f.setCutoffCurrent (amps);
    f.setEmphasis (1.0);
    const double nominal = std::fmax (f.nominalCutoffForCurrent (amps), 5.0);
    const int settle = int (calibrationRate * std::fmax (0.03, 10.0 / nominal));
    const int measure = int (calibrationRate * std::fmax (0.05, 20.0 / nominal));
    for (int i = 0; i < settle; ++i)
        f.process (i < 48 ? 1.0e-3 : 0.0);
    double prev = f.process (0.0), first = -1.0, last = -1.0, peak = 0.0;
    int crossings = 0;
    for (int i = 1; i < measure; ++i)
    {
        const double y = f.process (0.0);
        peak = std::fmax (peak, std::abs (y));
        if (prev < 0.0 && y >= 0.0)
        {
            const double at = double (i) - y / (y - prev); // interpolated upward crossing
            if (crossings == 0) first = at;
            last = at;
            ++crossings;
        }
        prev = y;
    }
    if (crossings < 3 || peak < 1.0e-3)
        return 0.0;
    return calibrationRate * double (crossings - 1) / (last - first);
}

/** Whether a copy of the filter, kicked, builds up an oscillation with the
    emphasis loop's gain forced to `k`. */
bool regenerates (LadderFilter f, double amps, double k)
{
    auto params = f.params();
    params.noiseRms = 0.0;
    params.kOverride = k;
    f.setParams (params);
    f.prepare (calibrationRate, 1);
    f.setTemperature (298.15);
    f.setCutoffCurrent (amps);
    f.setEmphasis (1.0);
    const int n = int (0.045 * calibrationRate);
    double early = 0.0, late = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double y = std::abs (f.process (i < 48 ? 1.0e-4 : 0.0));
        if (i >= n / 3 && i < n / 2) early = std::fmax (early, y);
        if (i >= 5 * n / 6) late = std::fmax (late, y);
    }
    return late > early;
}
} // namespace

void Voice::build (uint64_t s)
{
    seed = s;
    Rng rng (seed);

    const ExpoConverterParams vcoExpoNominal; // Board 1 rev 2 values (see ExpoConverter.h)
    for (int i = 0; i < 3; ++i)
    {
        vcoExpo[size_t (i)].setParams (ExpoConverterParams::drawn (rng, vcoExpoNominal));
        vcos[size_t (i)].setParams (VcoParams::drawn (rng, VcoParams {}));
    }

    // The mixer node is the bottom pair's input itself (R54, see Mixer.h):
    // the drive into the ladder follows from the schematic values.
    ladder.setParams (LadderParams::drawn (rng, LadderParams {}));

    mixer.setParams (MixerParams::drawn (rng, MixerParams {}));
    filterContour.setParams (ContourParams::drawn (rng, ContourParams::filter()));
    loudnessContour.setParams (ContourParams::drawn (rng, ContourParams::loudness()));
    vca.setParams (VcaParams::drawn (rng, VcaParams {}));
    glide.setParams (GlideParams::drawn (rng, GlideParams {}));
    extPreamp.setParams (ExtPreampParams::drawn (rng, ExtPreampParams {}));
    drawPanel (rng);
    // The modulation amplifier's standing offset follows Q18's gain (2N3392:
    // 150 to 300 about the model's 210).
    modOffset = rng.tolerance (ModulationMix::outputOffset, 0.30, 0.40);

    // The bench: what the service manual's adjustment procedures (section 5,
    // and R. J. Folkman's field service manual) have a technician do.
    calibrateOscillators();
    calibrateFilter();
    calibrateNoise();
}

void Voice::drawPanel (Rng& rng)
{
    // RANGE: the octave string and each oscillator's 15K RNG input (R43, R52,
    // R179: a matched set, like R25..R28). The OCTAVE trimmer scales the whole
    // string. Folkman's procedure (7/73, the revision 2 board's time):
    // "Turn OSCILLATOR 1 Mixer switch on and set RANGE to 2 ft. Turn on A-440.
    // Depress and hold second "A" key from the bottom. Adjust TUNE control for
    // zero beats. Switch RANGE to 8 ft. Depress same key and adjust Octave
    // Range trimpot ... for zero beats": 2' and 8' two octaves apart. (The
    // later service manual, 5.34, sets 16' against 2' instead.) What the other
    // steps and the other two oscillators then show is the parts' mismatch.
    std::array<double, 6> tap {};
    tap[1] = rng.tolerance (FrontPanel::rLo, 0.01) / FrontPanel::rOctave; // LO to 32': R29, 1 %
    for (size_t r = 2; r < 6; ++r)
        tap[r] = tap[r - 1] + rng.tolerance (1.0, 0.001);                 // R25..R28, matched to 0.1 %
    std::array<double, 3> input {};
    for (auto& g : input)
        g = rng.tolerance (1.0, 0.001);
    const double octaveTrim = 2.0 / (input[0] * (tap[5] - tap[3])) * (1.0 + 0.3 / 2400.0 * rng.normal());
    for (size_t i = 0; i < 3; ++i)
        for (size_t r = 0; r < 6; ++r)
            rangeCv[i][r] = FrontPanel::rangeOctaves[5] + input[i] * octaveTrim * (tap[r] - tap[5]);

    // Keyboard: 43 resistors of 10 ohm 1 % under a constant current (Fig. 9-7).
    // The oscillators are scaled on the keys themselves (low A, high A), so
    // those two keys are exact and the others carry the string's unevenness.
    // Notes beyond the 44 keys continue it evenly.
    std::array<double, highestKey - lowestKey + 1> semis {};
    for (size_t k = 1; k < semis.size(); ++k)
        semis[k] = semis[k - 1] + rng.tolerance (1.0, 0.01);
    const size_t kLow = size_t (lowA - lowestKey), kHigh = size_t (highA - lowestKey);
    const double origin = semis[kLow], step = double (highA - lowA) / (semis[kHigh] - semis[kLow]);
    for (auto& v : semis)
        v = (v - origin) * step + double (lowA - lowestKey);
    for (int n = 0; n < 128; ++n)
    {
        double st;
        if (n < lowestKey)       st = semis.front() + double (n - lowestKey);
        else if (n > highestKey) st = semis.back() + double (n - highestKey);
        else                     st = semis[size_t (n - lowestKey)];
        keyCv[size_t (n)] = st / 12.0;
    }

    // TUNE and FREQUENCY: 5K carbon pots (20 %) against 5 % resistors.
    tuneSpan = FrontPanel::tuneOctaves (1.0, rng.tolerance (FrontPanel::rTunePot, 0.20))
             - FrontPanel::tuneOctaves (0.0, rng.tolerance (FrontPanel::rTunePot, 0.20));
    for (size_t i = 1; i < 3; ++i)
    {
        const double pot = rng.tolerance (FrontPanel::rFreqPot, 0.20);
        freqSpan[i] = FrontPanel::frequencyOctaves (1.0, pot) - FrontPanel::frequencyOctaves (0.0, pot);
    }

    // The filter's control resistors (5 %). Filter Scale is trimmed with
    // KEYBOARD CONTROL 1 and 2 on: those two together are the octave a volt.
    const double r53 = rng.tolerance (FrontPanel::rKbd1, 0.05), r54 = rng.tolerance (FrontPanel::rKbd2, 0.05);
    const double unit = r53 * r54 / (r53 + r54);
    kbd1Gain = unit / r53;
    kbd2Gain = unit / r54;
    cutoffGain = unit / rng.tolerance (FrontPanel::rCutoff, 0.05);
    contourGain = unit / rng.tolerance (FrontPanel::rContour, 0.05);
    filterModGain = unit / rng.tolerance (FrontPanel::rFilterMod, 0.05);
}

double Voice::oscFrequency (int osc, double cv) noexcept
{
    return vcos[size_t (osc)].frequencyForCurrent (vcoExpo[size_t (osc)].current (cv));
}

double Voice::filterCutoffHz (double cv) noexcept
{
    return ladder.nominalCutoffForCurrent (filterExpo.current (cv));
}

double Voice::filterSelfOscillationHz (double cv)
{
    return selfOscillationHz (ladder, filterExpo.current (cv));
}

bool Voice::filterRegenerates (double cv, double emphasis)
{
    return regenerates (ladder, filterExpo.current (cv), LadderFilter::loopGain (ladder.params(), emphasis));
}

/** TUNE, the PITCH wheel, RANGE and FREQUENCY: what the panel adds to an
    oscillator's control voltage (octaves; the wheel's range is the BEND
    setting, see README). */
double Voice::oscPanelCv (int osc, const PanelState& p) const noexcept
{
    const auto& o = p.osc[size_t (osc)];
    const double detune = osc > 0 ? freqSpan[size_t (osc)] * (o.frequency - 0.5) : 0.0;
    return tuneSpan * (p.tune - 0.5) + p.pitchWheel * p.bendRange / 12.0
         + rangeCv[size_t (osc)][size_t (o.range)] + detune;
}

/** Oscillator 3 with OSC. 3 CONTROL off: its RANGE and a 4.4 times wider
    FREQUENCY only (service manual 2.18). */
double Voice::osc3FreeCvFor (const PanelState& p) const noexcept
{
    const auto& o = p.osc[2];
    return rangeCv[2][size_t (o.range)] + FrontPanel::osc3WideRatio * freqSpan[2] * (o.frequency - 0.5)
         + keyboardZero + osc3FreeOctaves;
}

double Voice::oscFrequencyFor (int osc, const PanelState& p, int note) noexcept
{
    const double cv = osc == 2 && ! p.osc3Control ? osc3FreeCvFor (p)
                                                  : oscPanelCv (osc, p) + keyVolts (note) + keyboardZero;
    return oscFrequency (osc, cv);
}

double Voice::filterCv (const PanelState& p, int note, double contourVolts) const noexcept
{
    return cutoffGain * 20.0 * (p.cutoff - 0.5)
         + ((p.kbd1 ? kbd1Gain : 0.0) + (p.kbd2 ? kbd2Gain : 0.0)) * keyVolts (note)
         + contourGain * p.contourAmount * contourVolts;
}

void Voice::calibrateOscillators()
{
    // Oscillator tuning at 25 C (Folkman; service manual Table 5-3, 5.31 to
    // 5.33): RANGE at 2', "Adjust Oscillator 1 Range Trimpot for 3520 Hz while
    // holding high A key down. Depress low A and hold while adjusting
    // Oscillator 1 Scale Trimpot for 440 Hz. Repeat until no further
    // improvement is attainable." The board has no other trimmer: the
    // high-frequency compensation is the fixed R42.
    const double high = double (highA - 69) / 12.0 + FrontPanel::rangeOctaves[5]; // CV of high A at 2'
    const double low = double (lowA - 69) / 12.0 + FrontPanel::rangeOctaves[5];
    Rng rng (seed ^ 0xCA1Bull);
    for (int i = 0; i < 3; ++i)
    {
        auto& ex = vcoExpo[size_t (i)];
        ex.setTemperatures (298.15, 298.15, 298.15);
        double scale = ex.scaleTrim(), offset = ex.offsetTrim();
        for (int it = 0; it < 80; ++it)
        {
            offset -= std::log2 (oscFrequency (i, high) / 3520.0);
            ex.setTrims (scale, offset);
            scale /= std::log2 (oscFrequency (i, high) / oscFrequency (i, low)) / (high - low);
            ex.setTrims (scale, offset);
        }
        // Zero beats by ear are good to about half a cent.
        offset += 0.5 / 1200.0 * rng.normal();
        scale *= 1.0 + 0.5 / 1200.0 / (high - low) * rng.normal();
        ex.setTrims (scale, offset);
    }
}

void Voice::calibrateFilter()
{
    filterExpo.setTemperature (298.15);
    ladder.setTemperature (298.15);
    Rng rng (seed ^ 0xF117ull);
    // The standing current for 440 Hz in the small-signal approximation: where
    // the bench starts from.
    double i440 = 4.0 * ladder.params().c[0] * phys::thermalVoltage (298.15) * 2.0 * M_PI * 440.0;

    // Regeneration Cal (Folkman; Table 5-4): "Set CUTOFF FREQUENCY control to
    // -1 ... Regeneration should start when the EMPHASIS control is between 7
    // and 8. If it does not, set EMPHASIS control to 7.5 and rotate the
    // Regeneration Cal. trimpot slowly clockwise until regeneration starts."
    // The loop gain this unit's ladder needs is found by trial, then R73 gives
    // it at 7.5 (the technician stops within a few percent of the threshold).
    double lo = 3.0, hi = 7.0;
    for (int it = 0; it < 9; ++it)
    {
        const double mid = 0.5 * (lo + hi);
        (regenerates (ladder, i440, mid) ? hi : lo) = mid;
    }
    {
        auto params = ladder.params();
        const double wanted = 0.5 * (lo + hi) * (1.0 + 0.02 * std::abs (rng.normal()));
        params.regenCal = 0.0;
        const double atZero = LadderFilter::loopGain (params, 0.75);
        params.regenCal = 1.0;
        const double atFull = LadderFilter::loopGain (params, 0.75);
        params.regenCal = std::clamp ((wanted - atZero) / (atFull - atZero), 0.0, 1.0);
        ladder.setParams (params);
    }

    // Filter Scale (5.14): KEYBOARD CONTROL 1 and 2 on, the filter singing,
    // "Depress the third A ... adjust the CUTOFF FREQUENCY control for 1760 Hz
    // ... Depress low A and adjust the Filter Scale trim potentiometer for zero
    // beat": two octaves of keyboard for two octaves of pitch. The pitch the
    // filter sings at does not follow its standing current exactly, so the
    // trim is set on what is heard.
    // A first setting from the ladder alone: how many octaves of pitch an
    // octave of standing current gives.
    const double f1 = selfOscillationHz (ladder, i440), f2 = selfOscillationHz (ladder, 4.0 * i440);
    const double exponent = f1 > 0.0 && f2 > 0.0 ? std::log2 (f2 / f1) / 2.0 : 1.0; // octaves of pitch per octave of current
    double scale = 1.0 / exponent;
    const double cvAtMinusOne = cutoffGain * 20.0 * (laws::cutoffKnobAt (laws::cutoffCalMark) - 0.5);
    double i0 = i440 * std::exp2 (-cvAtMinusOne * scale);
    filterExpo.setTrims (i0, scale);

    // Then both trims as the technician sets them, through the keys, the
    // CUTOFF knob and the cutoff converter (whose octaves stretch towards the
    // top), repeated "until the filter will track three octaves".
    auto sings = [this] (const PanelState& q, int note) { return selfOscillationHz (ladder, filterExpo.current (filterCv (q, note, 0.0))); };
    constexpr int thirdA = 69;
    for (int rep = 0; rep < 3; ++rep)
    {
        // Filter Scale: the third A tuned to 1760 Hz with CUTOFF, then low A to
        // a quarter of that with the trim
        PanelState q;
        q.kbd1 = q.kbd2 = true;
        q.cutoff = 0.5;
        for (int it = 0; it < 3; ++it)
        {
            const double f = sings (q, thirdA);
            if (f <= 0.0) break;
            q.cutoff += std::log2 (1760.0 / f) / 10.0;
        }
        const double fThird = sings (q, thirdA), fLow = sings (q, lowA);
        if (fThird > 0.0 && fLow > 0.0)
            scale *= 2.0 / std::log2 (fThird / fLow);
        // Filter Range: KEYBOARD CONTROL off, CUTOFF at -1, 440 Hz
        PanelState r;
        r.cutoff = laws::cutoffKnobAt (laws::cutoffCalMark);
        filterExpo.setTrims (i0, scale);
        for (int it = 0; it < 2; ++it)
        {
            const double f = sings (r, lowA);
            if (f <= 0.0) break;
            i0 *= std::pow (440.0 / f, 1.0 / exponent);
            filterExpo.setTrims (i0, scale);
        }
    }
    // Zero beats by ear: the technician stops within a cent or so.
    filterExpo.setTrims (i0 * std::exp2 (1.0 / 1200.0 * rng.normal()), scale * (1.0 + 1.0 / 2400.0 * rng.normal()));
}

void Voice::calibrateNoise()
{
    // Noise Level (Folkman; 2.5, 5.27): "Noise level should be -5 +/- 3dB. If
    // not, adjust noise level trim potentiometer ... so that both white and
    // pink fall within specification", read on the AC voltmeter at the HIGH
    // MAIN OUTPUT with the controls as section 5 leaves them: NOISE VOLUME and
    // main VOLUME fully clockwise, CUTOFF fully clockwise, EMPHASIS 0, the
    // loudness contour sustained at 10. The pink noise comes out a few dB above
    // the white: the trim centres the two on -5 dB. The manual's readings fit a
    // meter whose 0 dB is 0.775 V (the A-440's -8 dB and one triangle's +1 dB
    // follow from the circuit with it).
    constexpr double target = 0.775 * 0.5623; // -5 dB
    constexpr double probe = 0.02;            // V rms: small enough for the chain to be linear
    Mixer m = mixer;
    for (int ch = 0; ch < MixerParams::numChannels; ++ch)
        m.setChannel (ch, 1.0, ch == 3);
    auto params = ladder.params();
    params.noiseRms = 0.0;
    const auto& lc = loudnessContour.params();
    const double sustained = std::fmin (lc.sustainCap (1.0), lc.peakOutput() + lc.vOutDrop) - lc.vOutDrop;
    // What the meter reads for a white level (V rms at the board): white and
    // pink.
    auto reading = [&] (double level, double seconds) {
        const int total = int (seconds * calibrationRate), skip = int (0.03 * calibrationRate);
        NoiseSource n;
        n.prepare (calibrationRate, seed * 13 + 5);
        n.setWhiteLevel (level);
        std::array<double, 2> sum {}; // white, pink
        for (size_t colour = 0; colour < 2; ++colour)
        {
            LadderFilter f = ladder;
            f.setParams (params);
            f.prepare (calibrationRate, 1);
            f.setTemperature (298.15);
            f.setCutoffCurrent (filterExpo.current (cutoffGain * 10.0));
            f.setEmphasis (0.0);
            Vca v = vca;
            v.prepare (calibrationRate);
            for (int i = 0; i < total; ++i)
            {
                double white, pinkN, redN;
                n.process (white, pinkN, redN);
                const double source = colour == 0 ? white : pinkN;
                const double y = v.process (f.process (m.process ({ 0.0, 0.0, 0.0, source, 0.0 })), sustained) * jackGain();
                if (i >= skip)
                    sum[colour] += y * y;
            }
        }
        return std::array<double, 2> { std::sqrt (sum[0] / double (total - skip)),
                                       std::sqrt (sum[1] / double (total - skip)) };
    };
    auto centre = [] (const std::array<double, 2>& r) { return std::sqrt (r[0] * r[1]); }; // the two averaged in dB
    // A first setting where the chain is linear, then the trim turned with
    // the meter read at the real level: the wideband white noise drives the
    // ladder's input pair into its curve, and comes out lower than the
    // small-signal gain says.
    const double small = centre (reading (probe, 0.1));
    whiteLevel = small > 0.0 ? probe * target / small : 0.1;
    std::array<double, 2> r {};
    for (int it = 0; it < 2; ++it)
    {
        r = reading (whiteLevel, 0.25);
        const double c = centre (r);
        if (c <= 0.0) return;
        whiteLevel *= target / c;
        r[0] *= target / c; // what the meter now reads, the chain being nearly
        r[1] *= target / c; // linear over so small a turn
    }
    // "... so that both white and pink fall within specification": where the
    // two lie close to 6 dB apart on a unit (the filter's top end decides how
    // much of the white noise reaches the jack), centring can leave one at
    // the edge of -5 +/- 3 dB, and the trim goes the little further that
    // brings it a tenth of a decibel inside.
    constexpr double low = 0.775 * 0.3981 * 1.012, high = 0.775 * 0.7943 / 1.012; // -8 and -2 dB, less 0.1 dB
    if (r[1] > high)      whiteLevel *= high / r[1];
    else if (r[0] < low)  whiteLevel *= low / r[0];
}

void Voice::prepare (double oversampledRate)
{
    fs = oversampledRate;
    for (int i = 0; i < 3; ++i)
        vcos[size_t (i)].prepare (fs, seed * 31 + uint64_t (i));
    ladder.prepare (fs, seed * 17 + 3);
    noise.prepare (fs, seed * 13 + 5);
    noise.setWhiteLevel (whiteLevel);
    filterContour.prepare (fs);
    loudnessContour.prepare (fs);
    trigger.prepare (fs);
    vca.prepare (fs);
    glide.prepare (fs);
    extPreamp.prepare (fs);
    drift.prepare (fs / controlInterval, seed * 7 + 11);
    reset();
}

void Voice::reset()
{
    ladder.reset();
    filterContour.reset();
    loudnessContour.reset();
    trigger.reset();
    vTrigger = false;
    vca.reset();
    glide.reset (keyVolts (targetNote));
    extPreamp.reset();
    gate = false;
    firstNote = true;
    controlCounter = 0;
    lastOut = feedbackSample = osc3Last = 0.0;
}

void Voice::noteOn (int note, bool retrigger) noexcept
{
    targetNote = note;
    if (firstNote)
    {
        // The hold capacitor starts on the first key ever played (no glide from nowhere).
        glide.reset (keyVolts (note));
        firstNote = false;
    }
    // The contours start on the trigger section's V-trigger (render), not on
    // the key. A key arriving while another is held is the bus staying high
    // (no new trigger), except a poly voice taken over while it sounds,
    // which restarts its contours at once (no single-keyboard equivalent).
    const bool wasDown = gate;
    gate = true;
    if (retrigger && wasDown)
    {
        filterContour.gateOn();
        loudnessContour.gateOn();
    }
}

void Voice::noteOff() noexcept
{
    gate = false; // the contours are released at the V-trigger's fall
}

bool Voice::isActive() const noexcept
{
    return gate || vTrigger || loudnessContour.currentStage() != ContourGen::Stage::Idle
        || std::abs (lastOut) > 1.0e-4;
}

void Voice::controlUpdate (const PanelState& p) noexcept
{
    drift.setAmount (p.drift);
    drift.tick();
    for (int i = 0; i < 3; ++i)
    {
        const double tPair = drift.temperature (DriftModel::Vco1 + i);
        vcoExpo[size_t (i)].setTemperatures (tPair, tPair + drift.pairDifference (i),
                                             drift.temperature (DriftModel::Tempco1 + i));
    }
    const double tf = drift.temperature (DriftModel::FilterPair);
    filterExpo.setTemperature (tf);
    ladder.setTemperature (tf);
    vca.setTemperature (tf);

    osc3Kbd = p.osc3Control;
    for (int i = 0; i < 3; ++i)
    {
        oscStaticCv[size_t (i)] = oscPanelCv (i, p);
        vcos[size_t (i)].setWaveform (p.osc[size_t (i)].wave, i == 2);
    }
    osc3FreeCv = osc3FreeCvFor (p);

    mixer.setSourceResistance (0, vcos[0].sourceResistance());
    mixer.setSourceResistance (2, vcos[1].sourceResistance());
    mixer.setSourceResistance (4, vcos[2].sourceResistance());
    mixer.setChannel (0, p.osc[0].volume, p.osc[0].on);
    // EXTERNAL INPUT VOLUME sits ahead of the board 4 pre-amp; the mixer only
    // sees the pre-amp output through its ON switch.
    extPreamp.setVolume (p.extVolume);
    mixer.setChannel (1, 1.0, p.extOn);
    mixer.setChannel (2, p.osc[1].volume, p.osc[1].on);
    mixer.setChannel (3, p.noiseVolume, p.noiseOn);
    mixer.setChannel (4, p.osc[2].volume, p.osc[2].on);
    pink = p.noisePink;

    // The filter's control node: CUTOFF (R11 between +10 V and -10 V through
    // R55 200K: 10 octaves over the dial, "a rotation of approximately 2
    // divisions" for two octaves, 5.18), KEYBOARD CONTROL (R53, R54) and
    // AMOUNT OF CONTOUR (R17's wiper through R74 47K: 2.1 octaves per volt of
    // contour at 10).
    filterStaticCv = cutoffGain * 20.0 * (p.cutoff - 0.5);
    kbTrack = (p.kbd1 ? kbd1Gain : 0.0) + (p.kbd2 ? kbd2Gain : 0.0);
    contourOct = contourGain * p.contourAmount;
    ladder.setEmphasis (p.emphasis);

    filterContour.setAttack (p.fAttack);
    filterContour.setDecay (p.fDecay);
    filterContour.setSustain (p.fSustain);
    filterContour.setDecaySwitch (p.decayOn);
    loudnessContour.setAttack (p.lAttack);
    loudnessContour.setDecay (p.lDecay);
    loudnessContour.setSustain (p.lSustain);
    loudnessContour.setDecaySwitch (p.decayOn);

    glide.setTime (p.glide);
    glide.setEnabled (p.glideOn);

    // The modulation line: the amplifier through R57 against the wheel (a
    // rheostat to ground) and the loads the switches connect: the oscillators'
    // MOD inputs (oscillator 3's only while OSC. 3 CONTROL is on) and R52.
    // MOD. AMOUNT sets a stored minimum wheel position. Depths in octaves per
    // volt of the amplifier's output (`mod`).
    const double rWheel = modWheelOhms (std::max (p.modWheel, p.modAmount));
    double gLine = rWheel > 0.0 ? 1.0 / rWheel : 1.0e12;
    if (p.oscModulation) gLine += (osc3Kbd ? 3.0 : 2.0) / rOscModIn;
    if (p.filterModulation) gLine += 1.0 / rFilterModLoad;
    const double rLine = 1.0 / gLine;
    const double lineDivider = rLine / (rModOut + rLine); // line volts per amplifier volt
    oscModDepth = p.oscModulation ? lineDivider : 0.0;      // octaves per amplifier volt
    filterModDepth = p.filterModulation ? lineDivider * filterModGain : 0.0;
    std::tie (modOsc3, modNoise) = ModulationMix::amplifier (p.modMix, vcos[2].sourceResistance());
    modFloor = ModulationMix::outputFloor (rLine);
    feedback = p.feedbackPatch;
    volume = p.mainOn ? OutputVolume::gain (p.volume) : 0.0; // R75 and R20, SW19
}

void Voice::render (const PanelState& p, double* out, const double* extIn, int n, const double* a440) noexcept
{
    for (int s = 0; s < n; ++s)
    {
        if (controlCounter-- <= 0)
        {
            controlCounter = controlInterval - 1;
            controlUpdate (p);
        }

        // Keyboard voltage through the track and hold: it only follows the key
        // (and glides) while a key is down.
        const double kbVolts = glide.process (keyVolts (targetNote), gate);
        const double kb = kbVolts + keyboardZero;

        // The trigger section: C7's delays between the key and the V-trigger.
        if (trigger.process (gate) != vTrigger)
        {
            vTrigger = ! vTrigger;
            if (vTrigger)
            {
                filterContour.gateOn();
                loudnessContour.gateOn();
            }
            else
            {
                filterContour.gateOff();
                loudnessContour.gateOff();
            }
        }

        // Contours: output voltages. The filter contour goes through AMOUNT OF
        // CONTOUR to the filter's control node; the loudness contour drives the
        // VCA's V->I converter Q18 directly.
        const double fContour = filterContour.process();
        const double lContour = loudnessContour.process();

        // Noise. The WHITE / PINK switch is double-pole: WHITE gives white noise
        // to the mixer and pink noise to the modulation bus; PINK gives pink
        // to the mixer and red noise to the modulation bus ("PINK OR RED NOISE"
        // into MODULATION MIX, Fig. 9-8 circuit 15).
        double white, pinkN, redN;
        noise.process (white, pinkN, redN);
        const double noiseOut = pink ? pinkN : white;
        const double noiseMod = pink ? redN : pinkN;

        // Modulation bus: MODULATION MIX between osc 3 and noise (R3 network)
        // the amplifier's output (V): it inverts, and cannot pull far below zero
        const double mod = ModulationMix::limit (-(modOsc3 * osc3Last + modNoise * noiseMod) + modOffset, modFloor);

        // Oscillators (osc 3 first: it is the modulation source, and takes the
        // line like the others while OSC. 3 CONTROL is on)
        const double modCv = oscModDepth * mod;
        const double cv3 = osc3Kbd ? oscStaticCv[2] + kb + modCv : osc3FreeCv;
        const double o3 = vcos[2].process (vcoExpo[2].current (cv3));
        const double o1 = vcos[0].process (vcoExpo[0].current (oscStaticCv[0] + kb + modCv));
        const double o2 = vcos[1].process (vcoExpo[1].current (oscStaticCv[1] + kb + modCv));
        osc3Last = o3;

        // Mixer -> filter
        const double ext = extPreamp.process ((extIn ? extIn[s] : 0.0) + (feedback ? feedbackSample : 0.0));
        const double mixed = mixer.process ({ o1, ext, o2, noiseOut, o3 });

        const double fcv = filterStaticCv + contourOct * fContour + kbTrack * kbVolts + filterModDepth * mod;
        ladder.setCutoffCurrent (filterExpo.current (fcv));
        const double filtered = ladder.process (mixed);

        // VCA, output stage and VOLUME: volts at the main output jack
        const double y = vca.process (filtered, lContour, a440 ? a440[s] : 0.0) * volume;
        feedbackSample = y;
        lastOut = y;
        out[s] += y;
    }
}

} // namespace mm
