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

#include "SynthEngine.h"

#include <algorithm>
#include <cmath>

namespace mm
{

void SynthEngine::build (uint64_t unitSeed)
{
    // Drawing and calibrating eight units takes a moment (the bench renders of
    // Voice::build): nothing to do when the units asked for are already there.
    if (built && unitSeed == builtSeed)
        return;
    built = true;
    builtSeed = unitSeed;
    for (int i = 0; i < maxVoices; ++i)
        voices[size_t (i)].build (unitSeed * 1000003ull + uint64_t (i) * 7919ull + 1ull);
    // A-440 Adjustment (service manual 5.7): the trim is set "for 440 +/- 1Hz".
    Rng rng (unitSeed ^ 0xA440ull);
    refHz = 440.0 + 0.3 * rng.normal();
}

void SynthEngine::prepare (double hostSampleRate, int maxBlockSize, int oversampling)
{
    hostRate = hostSampleRate;
    maxBlock = std::max (1, maxBlockSize);
    osFactor = oversampling >= 8 ? 8 : oversampling >= 4 ? 4 : oversampling >= 2 ? 2 : 1;
    decimator.prepare (osFactor);
    interpolator.prepare (osFactor);
    osBuffer.assign (size_t (maxBlock * osFactor), 0.0);
    extBuffer.assign (size_t (maxBlock * osFactor), 0.0);
    for (auto& v : voices)
        v.prepare (hostRate * osFactor);
    // Output coupling capacitor: ~3 Hz high-pass
    dcCoeff = std::exp (-2.0 * M_PI * 3.0 / hostRate);
    smoothCoeff = 1.0 - std::exp (-double (smoothChunk) / (0.010 * hostRate));
    dcState = dcPrevIn = 0.0;
}

void SynthEngine::setVoiceMode (bool poly, int n) noexcept
{
    if (poly != alloc.poly() || (poly && n != alloc.activeVoiceCount()))
    {
        alloc.setPoly (poly, n);
        polyGain = poly ? 1.0 / std::sqrt (double (std::max (1, alloc.activeVoiceCount()))) : 1.0;
        for (auto& v : voices) v.noteOff();
        alloc.consumeAllOff();
    }
}

void SynthEngine::apply (const VoiceCommand& c) noexcept
{
    if (c.voice < 0) return;
    auto& v = voices[size_t (c.voice)];
    if (c.gate)
        v.noteOn (c.note, c.retrigger);
    else
        v.noteOff();
}

void SynthEngine::noteOn (int note, float velocity) noexcept
{
    VoiceCommand cmd[2];
    const int n = alloc.noteOn (note, velocity, cmd);
    for (int i = 0; i < n; ++i) apply (cmd[i]);
}

void SynthEngine::noteOff (int note) noexcept
{
    VoiceCommand cmd[2];
    const int n = alloc.noteOff (note, cmd);
    for (int i = 0; i < n; ++i) apply (cmd[i]);
}

void SynthEngine::allNotesOff() noexcept
{
    alloc.allNotesOff();
    alloc.consumeAllOff();
    for (auto& v : voices) v.noteOff();
}

void SynthEngine::process (float* out, const float* in, int n) noexcept
{
    int done = 0;
    while (done < n)
    {
        const int chunk = std::min ({ maxBlock, smoothChunk, n - done });
        panel.smoothToward (target, smoothCoeff);
        const int m = chunk * osFactor;
        std::fill (osBuffer.begin(), osBuffer.begin() + m, 0.0);

        const double* ext = nullptr;
        if (in != nullptr)
        {
            // External input: volts = sample * 4 V, brought to the oversampled
            // rate by the half-band interpolator.
            for (int i = 0; i < chunk; ++i)
                interpolator.process (double (in[done + i]) / inputScale, extBuffer.data() + size_t (i * osFactor));
            ext = extBuffer.data();
        }

        const int active = alloc.activeVoiceCount();
        double brightest = -1.0;
        for (int v = 0; v < active; ++v)
            if (voices[size_t (v)].isActive())
            {
                voices[size_t (v)].render (panel, osBuffer.data(), ext, m);
                brightest = std::max (brightest, voices[size_t (v)].overloadLamp());
            }
        // No unit sounding: the lamp filament simply cools down.
        lamp = brightest >= 0.0 ? brightest : lamp * std::exp (-double (chunk) / (hostRate * 0.06));

        const double refInc = refHz / hostRate;
        for (int i = 0; i < chunk; ++i)
        {
            double y = decimator.process (osBuffer.data() + size_t (i * osFactor));
            // Output coupling capacitor (DC block)
            const double hp = y - dcPrevIn + dcCoeff * dcState;
            dcPrevIn = y;
            dcState = hp;
            y = hp * outputScale * polyGain;
            if (panel.a440)
            {
                // A-440 reference (board 4, circuit 13): a Wien bridge oscillator
                // whose output goes through R40 and C8 into the second VCA's
                // load, ahead of the output stage: after both VCAs, so it sounds
                // with no key down and follows the VOLUME pot and the OUTPUT
                // switch. "Check to see that output is -8 +/- 2dB" at the HIGH
                // MAIN OUTPUT (5.7; 0 dB = 0.775 V): 0.436 V peak with VOLUME at
                // 10. "It produces a somewhat imperfect sinewave which increases
                // its harmonic content. These harmonics or overtones assist in
                // tuning" an octave or two away (Folkman): the diode limiter
                // CR3 / CR4; the harmonics' levels are estimates.
                refPhase += refInc;
                if (refPhase >= 1.0) refPhase -= 1.0;
                const double th = 2.0 * M_PI * refPhase;
                const double wave = std::sin (th) + 0.03 * std::sin (2.0 * th) + 0.015 * std::sin (3.0 * th)
                                  + 0.007 * std::sin (4.0 * th);
                const double atFull = panel.mainOn ? OutputVolume::gain (panel.volume) / OutputVolume::gain (1.0) : 0.0;
                y += a440Peak * atFull * outputScale * wave;
            }
            out[done + i] = float (y);
        }
        done += chunk;
    }
}

} // namespace mm
