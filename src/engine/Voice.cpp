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
//   keyboard CV         = (note - 60) / 12 for the filter tracking reference
constexpr double rangeOffset[] = { -7.0, -2.0, -1.0, 0.0, 1.0, 2.0 }; // LO, 32', 16', 8', 4', 2'
constexpr double detuneSemis = 7.0;      // osc 2/3 FREQUENCY pot span (+/-)
constexpr double tuneSemis = 1.5;        // master TUNE span (+/-)
constexpr double oscModOctaves = 1.25;   // modulation injection, oscillators
constexpr double filterModOctaves = 2.0; // modulation injection, filter (assumed)
constexpr double contourOctaves = 4.0;   // AMOUNT OF CONTOUR at 10
constexpr double oscPeak = 1.7;          // waveshaper output peak (V), Fig. 9-3
// Noise level reaching the mixer, relative to the level on the modulation
// bus. The noise channel's 11 k summing resistor (R48) is 3x the conductance
// of the oscillators' 33 k; the generator's output level is not documented,
// so this keeps the noise/oscillator balance of the previous ear-tuned model
// (1/3 with 2.5 V oscillators, scaled for the 1.7 V of Fig. 9-3).
constexpr double noiseToMixer = (1.0 / 3.0) * (1.7 / 2.5);

double cutoffCv (double pos) { return -5.5 + 10.5 * pos; } // ~14 Hz .. ~23 kHz
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
    filterContour.setParams (ContourParams::drawn (rng, ContourParams {}));
    loudnessContour.setParams (ContourParams::drawn (rng, ContourParams {}));
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
            cv += (o.frequency - 0.5) * 2.0 * detuneSemis / 12.0;
        oscStaticCv[size_t (i)] = cv;
        vcos[size_t (i)].setWaveform (o.wave, i == 2);
    }

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

    const double wheel = taper::audio (p.modWheel); // 50k audio-taper mod wheel
    oscModDepth = p.oscModulation ? wheel * oscModOctaves : 0.0;
    filterModDepth = p.filterModulation ? wheel * filterModOctaves : 0.0;
    std::tie (modOsc3, modNoise) = ModulationMix::gains (p.modMix);
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

        // Contours
        const double fEg = filterContour.process() / 5.0;
        const double lEg = loudnessContour.process() / 5.0;

        // Noise. The WHITE / PINK switch is double-pole: WHITE gives white noise
        // to the mixer and pink noise to the modulation bus; PINK gives pink
        // to the mixer and red noise to the modulation bus ("PINK OR RED NOISE"
        // into MODULATION MIX, Fig. 9-8 circuit 15).
        double white, pinkN, redN;
        noise.process (white, pinkN, redN);
        const double noiseOut = pink ? pinkN : white;
        const double noiseMod = NoiseSource::modulationGain * (pink ? redN : pinkN);

        // Modulation bus: MODULATION MIX between osc 3 and noise (R3 network)
        const double mod = modOsc3 * (osc3Last / oscPeak) + modNoise * (noiseMod / oscPeak);

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

        const double fcv = filterStaticCv + contourOct * fEg + kbTrack * (kb + 9.0 / 12.0)
                         + filterModDepth * mod;
        ladder.setCutoffCurrent (filterExpo.current (fcv));
        const double filtered = ladder.process (mixed);

        // VCA and output
        const double y = vca.process (filtered, lEg) * volume;
        feedbackSample = y;
        lastOut = y;
        out[s] += y;
    }
}

} // namespace mm
