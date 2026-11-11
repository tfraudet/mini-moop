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

#include <array>
#include <utility>

#include <algorithm>
#include <tuple>
#include <cmath>

namespace mm
{

namespace
{
// Control-voltage conventions (1 V/oct everywhere):
//   oscillator CV 0 V   = A4 (440 Hz) on the 8' range
//   filter CV 0 V       = 640 Hz cutoff
//   keyboard CV         = 0 V on the lowest key, F2 (MIDI 41): the key string's
//                         grounded end (Fig. 9-7), for the filter's KEYBOARD CONTROL
// LO: 6.74 octaves below 8', as measured on the CA-72 project's hardware
// reference (oscillator 3, OSC. 3 CONTROL off and on: 6.732 and 6.745).
constexpr double rangeOffset[] = { -6.74, -2.0, -1.0, 0.0, 1.0, 2.0 }; // LO, 32', 16', 8', 4', 2'
constexpr double keyboardZero = (41.0 - 69.0) / 12.0; // the lowest key's CV against A4's
constexpr double tuneSemis = 1.5;        // master TUNE span (+/-)
// The modulation line (board 3 circuit 15, Fig. 9-8; left hand controller,
// Fig. 9-12). MODULATION MIX drives an inverting amplifier (ModulationMix);
// fully at oscillator 3, its triangle comes out at 0.825 of its swing,
// inverted (ngspice: +0.84 / -0.79, the PNP output stage only sources). The
// line has the keyboard's sense (the MOD inputs share the oscillators' summer
// with it), so a rising oscillator 3 lowers the pitch and the cutoff. The
// output reaches the line through R57 1K; the MODULATION wheel R1402 is a
// rheostat from the line to ground, 0 at the back and 1.2K fully forward. The
// oscillators take the line through 51.1K MOD inputs (as the keyboard's R27: 1
// octave a volt; three of them load it), the filter's control node through R52
// 33K, where KEYBOARD CONTROL 1 + 2 (R53 300K || R54 150K = 100K) make 1 octave
// a volt: 100K / 33K as deep. At full wheel the oscillators swing +-0.74 octave.
constexpr double rModOut = 1.0e3;               // R57
constexpr double rWheelFull = 1.2e3;            // R1402 fully forward (Fig. 9-12)
constexpr double rOscModLoad = 51.1e3 / 3.0;    // the three oscillators' MOD inputs
constexpr double rFilterModLoad = 33.0e3;       // R52
constexpr double filterModRatio = 100.0e3 / 33.0e3;

/** The MODULATION wheel's resistance (R1402, 50K audio, 90 degrees of its
    rotation used from the counter-clockwise stop): 1.2K fully forward (Fig.
    9-12); its shape over that travel is the generic audio taper's first third
    (an assumption, as in the CA-72 project). */
double modWheelOhms (double pos)
{
    const double w = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
    constexpr double third = 4.326748710922225; // 81^(1/3)
    return rWheelFull * (std::pow (third, w) - 1.0) / (third - 1.0);
}

constexpr double contourOctaves = 4.0;   // AMOUNT OF CONTOUR at 10
// Noise level reaching the mixer, relative to the level on the modulation
// bus. The noise channel's 11 k summing resistor (R48) is 3x the conductance
// of the oscillators' 33 k; the generator's output level is not documented,
// so this keeps the noise/oscillator balance of the previous ear-tuned model
// (1/3 with 2.5 V oscillators, scaled for the 1.7 V of Fig. 9-3).
constexpr double noiseToMixer = (1.0 / 3.0) * (1.7 / 2.5);

using laws::piecewise;

/** Oscillators 2 and 3's FREQUENCY knobs (R4 / R5, 5K linear), as measured on
    the CA-72 project's hardware reference: semitones from the centre at the
    dial's -5 and +5 marks and at its stops (+-7.5 marks, the pot's ends).
    The knob 0..1 covers the dial's -7.5..+7.5. */
double frequencySemitones (int osc, double pos)
{
    static constexpr std::array<std::pair<double, double>, 5> osc2 { { { -7.5, -8.56 }, { -5.0, -6.48 }, { 0.0, 0.0 }, { 5.0, 6.61 }, { 7.5, 8.24 } } };
    static constexpr std::array<std::pair<double, double>, 5> osc3 { { { -7.5, -8.63 }, { -5.0, -6.73 }, { 0.0, 0.0 }, { 5.0, 6.92 }, { 7.5, 8.22 } } };
    const double mark = (pos - 0.5) * 15.0;
    return piecewise (osc == 1 ? osc2 : osc3, mark);
}

/** CUTOFF knob -> filter CV (octaves): the service manual's law
    (PanelLaws.h). `cutoffCalCv` is the Filter Range trim: the CV at the -1
    mark that makes this model's filter sing at 440 Hz there (EMPHASIS 10,
    KEYBOARD CONTROL off), averaged over units 1 to 4. */
double cutoffCv (double pos)
{
    constexpr double cutoffCalCv = -0.437;
    return cutoffCalCv + laws::cutoffOctaves (pos);
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

    // Filter cutoff converter (Board 4): its values have not been checked
    // against the schematic yet, so it keeps its own estimates.
    ExpoConverterParams filtExpoNominal;
    filtExpoNominal.iRef = 20.0e-6;
    filtExpoNominal.rIn = 100.0e3;
    filterExpo.setParams (ExpoConverterParams::drawn (rng, filtExpoNominal));

    // The mixer node is the bottom pair's input itself (R54, see Mixer.h):
    // the drive into the ladder now follows from the schematic values.
    ladder.setParams (LadderParams::drawn (rng, LadderParams {}));

    mixer.setParams (MixerParams::drawn (rng, MixerParams {}));
    filterContour.setParams (ContourParams::drawn (rng, ContourParams::filter()));
    loudnessContour.setParams (ContourParams::drawn (rng, ContourParams::loudness()));
    vca.setParams (VcaParams::drawn (rng, VcaParams {}));
    glide.setParams (GlideParams::drawn (rng, GlideParams {}));
    extPreamp.setParams (ExtPreampParams::drawn (rng, ExtPreampParams {}));

    calibrateOscillators();
    calibrateFilter();
}

double Voice::oscFrequency (int osc, double cv) noexcept
{
    return vcos[size_t (osc)].frequencyForCurrent (vcoExpo[size_t (osc)].current (cv));
}

double Voice::filterCutoffHz (double cv) noexcept
{
    return ladder.nominalCutoffForCurrent (filterExpo.current (cv));
}

void Voice::calibrateOscillators()
{
    // Bench calibration at 25 °C, as in the service manual procedure:
    // OFFSET at A4, V/OCT between A1 and A4, HF trim at A7.
    Rng rng (seed ^ 0xCA1Bull);
    for (int i = 0; i < 3; ++i)
    {
        auto& ex = vcoExpo[size_t (i)];
        ex.setTemperatures (298.15, 298.15, 298.15);
        double scale = ex.scaleTrim(), offset = ex.offsetTrim();
        for (int it = 0; it < 60; ++it)
        {
            offset -= std::log2 (oscFrequency (i, 0.0) / 440.0);
            ex.setTrims (scale, offset);
            const double oct = std::log2 (oscFrequency (i, 0.0) / oscFrequency (i, -3.0)) / 3.0;
            scale /= oct;
            ex.setTrims (scale, offset);
            // HF trim: d(log2 f)/d(hf) ~ ratio * I / (Vt ln2)
            const double err = std::log2 (oscFrequency (i, 3.0) / 3520.0);
            const double i3 = ex.current (3.0);
            const double sens = ex.attenuatorRatio() * i3 / (ex.thermalVoltage() * std::log (2.0));
            ex.setHfTrim (ex.hfTrim() - 0.7 * err / std::fmax (sens, 1e-12));
        }
        // A technician's ear is good to about half a cent.
        offset += 0.5 / 1200.0 * rng.normal();
        ex.setTrims (scale, offset);
    }
}

void Voice::calibrateFilter()
{
    filterExpo.setTemperatures (298.15, 298.15, 298.15);
    ladder.setTemperature (298.15);
    filterI0 = 4.0 * ladder.params().c[0] * phys::thermalVoltage (298.15) * 2.0 * M_PI * filterF0;
    filterExpo.calibrate (0.0, filterI0, 0.85);
}

void Voice::prepare (double oversampledRate)
{
    fs = oversampledRate;
    for (int i = 0; i < 3; ++i)
        vcos[size_t (i)].prepare (fs, seed * 31 + uint64_t (i));
    ladder.prepare (fs, seed * 17 + 3);
    noise.prepare (fs, seed * 13 + 5);
    noise.setWhiteLevel (0.8);
    filterContour.prepare (fs);
    loudnessContour.prepare (fs);
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
    glide.reset ((targetNote - 69) / 12.0);
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
        // Track & hold starts from the first key ever played (no glide from nowhere).
        glide.reset ((note - 69) / 12.0);
        firstNote = false;
    }
    gate = true;
    if (retrigger)
    {
        filterContour.gateOn();
        loudnessContour.gateOn();
    }
}

void Voice::noteOff() noexcept
{
    gate = false;
    filterContour.gateOff();
    loudnessContour.gateOff();
}

bool Voice::isActive() const noexcept
{
    return gate || loudnessContour.currentStage() != ContourGen::Stage::Idle
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
    filterExpo.setTemperatures (tf, tf + drift.pairDifference (3), drift.temperature (DriftModel::FilterTempco));
    ladder.setTemperature (tf);
    vca.setTemperature (tf);

    const double common = (p.tune - 0.5) * 2.0 * tuneSemis / 12.0 + p.pitchWheel * p.bendRange / 12.0;
    for (int i = 0; i < 3; ++i)
    {
        const auto& o = p.osc[size_t (i)];
        double cv = common + rangeOffset[int (o.range)];
        if (i > 0)
            cv += frequencySemitones (i, o.frequency) / 12.0;
        oscStaticCv[size_t (i)] = cv;
        vcos[size_t (i)].setWaveform (o.wave, i == 2);
    }

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

    filterStaticCv = cutoffCv (p.cutoff);
    kbTrack = (p.kbd1 ? 1.0 / 3.0 : 0.0) + (p.kbd2 ? 2.0 / 3.0 : 0.0);
    contourOct = contourOctaves * p.contourAmount;
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
    // rheostat to ground) and the loads the switches connect. MOD. AMOUNT sets
    // a stored minimum wheel position. Depths in octaves per volt of the
    // amplifier's output (`mod`).
    const double rWheel = modWheelOhms (std::max (p.modWheel, p.modAmount));
    double gLine = rWheel > 0.0 ? 1.0 / rWheel : 1.0e12;
    if (p.oscModulation) gLine += 1.0 / rOscModLoad;
    if (p.filterModulation) gLine += 1.0 / rFilterModLoad;
    const double rLine = 1.0 / gLine;
    const double lineDivider = rLine / (rModOut + rLine); // line volts per amplifier volt
    oscModDepth = p.oscModulation ? lineDivider : 0.0;      // octaves per amplifier volt
    filterModDepth = p.filterModulation ? lineDivider * filterModRatio : 0.0;
    std::tie (modOsc3, modNoise) = ModulationMix::amplifier (p.modMix, vcos[2].sourceResistance());
    osc3Kbd = p.osc3Control;
    feedback = p.feedbackPatch;
    volume = p.mainOn ? OutputVolume::gain (p.volume) : 0.0; // R20 linear divider, SW19
}

void Voice::render (const PanelState& p, double* out, const double* extIn, int n) noexcept
{
    for (int s = 0; s < n; ++s)
    {
        if (controlCounter-- <= 0)
        {
            controlCounter = controlInterval - 1;
            controlUpdate (p);
        }

        // Keyboard CV through the glide circuit
        const double kb = glide.process ((targetNote - 69) / 12.0);

        // Contours: output voltages. The filter contour reaches AMOUNT OF
        // CONTOUR's full span (contourOctaves) at 5 V; the loudness contour
        // drives the VCA's V->I converter Q18 directly.
        const double fEg = filterContour.process() / 5.0;
        const double lContour = loudnessContour.process();

        // Noise. The WHITE / PINK switch is double-pole: WHITE gives white noise
        // to the mixer and pink noise to the modulation bus; PINK gives pink
        // to the mixer and red noise to the modulation bus ("PINK OR RED NOISE"
        // into MODULATION MIX, Fig. 9-8 circuit 15).
        double white, pinkN, redN;
        noise.process (white, pinkN, redN);
        const double noiseOut = pink ? pinkN : white;
        const double noiseMod = NoiseSource::modulationGain * (pink ? redN : pinkN);

        // Modulation bus: MODULATION MIX between osc 3 and noise (R3 network)
        const double mod = -(modOsc3 * osc3Last + modNoise * noiseMod); // the amplifier's output (V): it inverts

        // Oscillators (osc 3 first: it is the modulation source)
        const double cv3 = oscStaticCv[2] + (osc3Kbd ? kb : 0.0);
        const double o3 = vcos[2].process (vcoExpo[2].current (cv3));
        const double modCv = oscModDepth * mod;
        const double o1 = vcos[0].process (vcoExpo[0].current (oscStaticCv[0] + kb + modCv));
        const double o2 = vcos[1].process (vcoExpo[1].current (oscStaticCv[1] + kb + modCv));
        osc3Last = o3;

        // Mixer -> filter
        const double ext = extPreamp.process ((extIn ? extIn[s] : 0.0) + (feedback ? feedbackSample : 0.0));
        const double mixed = mixer.process ({ o1, ext, o2, noiseToMixer * noiseOut, o3 });

        const double fcv = filterStaticCv + contourOct * fEg + kbTrack * (kb - keyboardZero)
                         + filterModDepth * mod;
        ladder.setCutoffCurrent (filterExpo.current (fcv));
        const double filtered = ladder.process (mixed);

        // VCA and output
        const double y = vca.process (filtered, lContour) * volume;
        feedbackSample = y;
        lastOut = y;
        out[s] += y;
    }
}

} // namespace mm
