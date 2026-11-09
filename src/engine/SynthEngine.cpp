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
    for (int i = 0; i < maxVoices; ++i)
        voices[size_t (i)].build (unitSeed * 1000003ull + uint64_t (i) * 7919ull + 1ull);
}

void SynthEngine::prepare (double hostSampleRate, int maxBlockSize, int oversampling)
{
    hostRate = hostSampleRate;
    maxBlock = std::max (1, maxBlockSize);
    osFactor = oversampling >= 8 ? 8 : oversampling >= 4 ? 4 : oversampling >= 2 ? 2 : 1;
    decimator.prepare (osFactor);
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
            // External input: volts = sample * 4 V, linear interpolation to the oversampled rate.
            for (int i = 0; i < chunk; ++i)
            {
                const float cur = in[done + i];
                for (int k = 0; k < osFactor; ++k)
                    extBuffer[size_t (i * osFactor + k)] = (lastIn + (cur - lastIn) * (k + 1) / double (osFactor)) / inputScale;
                lastIn = cur;
            }
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

        const double refInc = 440.0 / hostRate;
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
                // A-440 reference: injected into the output VCA, so it follows the
                // VOLUME pot and the OUTPUT ON switch like the voices do.
                refPhase += refInc;
                if (refPhase >= 1.0) refPhase -= 1.0;
                const double outGain = panel.mainOn ? OutputVolume::gain (panel.volume) : 0.0;
                y += 0.125 * outGain * std::sin (2.0 * M_PI * refPhase); // same level as before relative to the voices
            }
            out[done + i] = float (y);
        }
        done += chunk;
    }
}

} // namespace mm
