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
// Note handling.
//
// Mono mode reproduces the original keyboard: lowest-note priority, single
// trigger (the contours only restart when a key is pressed with no other key
// held), and the pitch falls back to the lowest key still held on release.
//
// Poly mode gives every voice the same single-trigger behaviour for its own
// note; voices are assigned round-robin to the longest-idle voice and the
// oldest sounding voice is stolen when all are busy.

#include <algorithm>
#include <array>
#include <cstdint>

namespace mm
{

struct VoiceCommand
{
    int voice = -1;
    int note = 60;
    bool gate = false;
    bool retrigger = false;
    float velocity = 1.0f;
};

class VoiceAllocator
{
public:
    static constexpr int maxVoices = 8;

    void setPoly (bool polyMode, int count) noexcept
    {
        const int n = polyMode ? std::clamp (count, 1, maxVoices) : 1;
        if (polyMode != isPoly || n != numVoices)
        {
            isPoly = polyMode;
            numVoices = n;
            allNotesOff();
        }
    }

    int activeVoiceCount() const noexcept { return numVoices; }
    bool poly() const noexcept { return isPoly; }

    void setSustainPedal (bool down) noexcept
    {
        sustain = down;
        if (! down)
            for (int n = 0; n < 128; ++n)
                if (sustained[size_t (n)] && ! held[size_t (n)])
                {
                    sustained[size_t (n)] = false;
                    releaseNote (n);
                }
    }

    /** Returns the number of commands written (0..2). */
    int noteOn (int note, float velocity, VoiceCommand* out) noexcept
    {
        held[size_t (note)] = true;
        sustained[size_t (note)] = false;
        stamp[size_t (note)] = ++clock;
        if (! isPoly)
        {
            const bool anyOther = lowestHeldExcept (note) >= 0 || anySustained (note);
            const int low = lowestSounding();
            out[0] = { 0, low, true, ! anyOther, velocity };
            monoNote = low;
            return 1;
        }

        int v = findVoiceForNote (note);
        if (v < 0) v = findFreeVoice();
        if (v < 0) v = findOldestVoice();
        auto& vs = voices[size_t (v)];
        vs.note = note;
        vs.gate = true;
        vs.age = clock;
        out[0] = { v, note, true, true, velocity };
        return 1;
    }

    int noteOff (int note, VoiceCommand* out) noexcept
    {
        held[size_t (note)] = false;
        if (sustain)
        {
            sustained[size_t (note)] = true;
            return 0;
        }
        return releaseNote (note, out);
    }

    void allNotesOff() noexcept
    {
        held.fill (false);
        sustained.fill (false);
        for (auto& v : voices) { v.gate = false; v.note = -1; }
        monoNote = -1;
        pendingRelease = true;
    }

    /** True once after allNotesOff(), so the engine can close every gate. */
    bool consumeAllOff() noexcept { const bool r = pendingRelease; pendingRelease = false; return r; }

private:
    struct VoiceState { int note = -1; bool gate = false; uint64_t age = 0; };

    int releaseNote (int note, VoiceCommand* out = nullptr) noexcept
    {
        if (! isPoly)
        {
            const int low = lowestSounding();
            if (low < 0)
            {
                monoNote = -1;
                if (out) out[0] = { 0, note, false, false, 0.0f };
                return out ? 1 : 0;
            }
            monoNote = low;
            if (out) out[0] = { 0, low, true, false, 1.0f };
            return out ? 1 : 0;
        }
        for (int v = 0; v < numVoices; ++v)
        {
            auto& vs = voices[size_t (v)];
            if (vs.gate && vs.note == note)
            {
                vs.gate = false;
                vs.age = ++clock;
                if (out) out[0] = { v, note, false, false, 0.0f };
                return out ? 1 : 0;
            }
        }
        return 0;
    }

    int lowestSounding() const noexcept
    {
        for (int n = 0; n < 128; ++n)
            if (held[size_t (n)] || sustained[size_t (n)]) return n;
        return -1;
    }
    int lowestHeldExcept (int note) const noexcept
    {
        for (int n = 0; n < 128; ++n)
            if (n != note && held[size_t (n)]) return n;
        return -1;
    }
    bool anySustained (int note) const noexcept
    {
        for (int n = 0; n < 128; ++n)
            if (n != note && sustained[size_t (n)]) return true;
        return false;
    }
    int findVoiceForNote (int note) const noexcept
    {
        for (int v = 0; v < numVoices; ++v)
            if (voices[size_t (v)].note == note) return v;
        return -1;
    }
    int findFreeVoice() const noexcept
    {
        int best = -1;
        uint64_t bestAge = UINT64_MAX;
        for (int v = 0; v < numVoices; ++v)
            if (! voices[size_t (v)].gate && voices[size_t (v)].age < bestAge) { best = v; bestAge = voices[size_t (v)].age; }
        return best;
    }
    int findOldestVoice() const noexcept
    {
        int best = 0;
        uint64_t bestAge = UINT64_MAX;
        for (int v = 0; v < numVoices; ++v)
            if (voices[size_t (v)].age < bestAge) { best = v; bestAge = voices[size_t (v)].age; }
        return best;
    }

    std::array<VoiceState, maxVoices> voices {};
    std::array<bool, 128> held {}, sustained {};
    std::array<uint64_t, 128> stamp {};
    uint64_t clock = 0;
    int numVoices = 1, monoNote = -1;
    bool isPoly = false, sustain = false, pendingRelease = false;
};

} // namespace mm
