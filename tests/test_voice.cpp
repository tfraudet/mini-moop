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

// Whole-voice / engine behaviour: sound comes out, levels are sane, no NaN
// at extreme settings, mono note priority, CPU budget.

#include "engine/SynthEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <cmath>
#include <vector>

using namespace mm;

namespace
{
struct Stats { double peak = 0.0, rms = 0.0; bool finite = true; };

Stats render (SynthEngine& e, int samples)
{
    std::vector<float> buf (static_cast<size_t> (samples));
    e.process (buf.data(), nullptr, samples);
    Stats s;
    double acc = 0.0;
    for (float v : buf)
    {
        if (! std::isfinite (v)) s.finite = false;
        s.peak = std::fmax (s.peak, std::abs (double (v)));
        acc += double (v) * v;
    }
    s.rms = std::sqrt (acc / samples);
    return s;
}

PanelState basicPatch()
{
    PanelState p;
    p.osc[1].on = false;
    p.osc[2].on = false;
    p.cutoff = 0.7;
    p.lSustain = 1.0;
    p.drift = 0.0;
    return p;
}
} // namespace

TEST_CASE ("A held note produces a sound of sensible level", "[voice]")
{
    SynthEngine e;
    e.build (1);
    e.prepare (48000.0, 512, 4);
    e.setPanel (basicPatch());
    e.noteOn (57, 1.0f);
    const auto s = render (e, 48000);
    INFO ("peak " << s.peak << " rms " << s.rms);
    CHECK (s.finite);
    CHECK (s.peak > 0.05);
    CHECK (s.peak < 1.0);
    e.noteOff (57);
    render (e, 48000);
    const auto tail = render (e, 4800);
    CHECK (tail.peak < 0.01);
}

TEST_CASE ("Extreme settings stay finite (overdriven mixer, full emphasis, fast mod)", "[voice]")
{
    SynthEngine e;
    e.build (2);
    e.prepare (48000.0, 256, 2);
    PanelState p;
    for (auto& o : p.osc) { o.volume = 1.0; o.on = true; o.range = Range::Ft2; o.wave = Waveform::NarrowRect; }
    p.noiseOn = true; p.noiseVolume = 1.0;
    p.emphasis = 1.0; p.cutoff = 1.0; p.contourAmount = 1.0;
    p.oscModulation = true; p.filterModulation = true; p.modWheel = 1.0;
    p.feedbackPatch = true; p.extOn = true; p.extVolume = 1.0;
    p.drift = 3.0;
    e.setPanel (p);
    e.noteOn (96, 1.0f);
    const auto s = render (e, 48000);
    CHECK (s.finite);
    CHECK (s.peak < 4.0);
}

TEST_CASE ("Filter self-oscillates with emphasis at 10 and no input", "[voice]")
{
    SynthEngine e;
    e.build (3);
    e.prepare (48000.0, 512, 4);
    PanelState p;
    for (auto& o : p.osc) o.on = false;
    p.emphasis = 1.0;
    p.cutoff = 0.6;
    p.drift = 0.0;
    e.setPanel (p);
    e.noteOn (60, 1.0f);
    render (e, 24000);
    const auto s = render (e, 24000);
    INFO ("self-oscillation peak " << s.peak);
    CHECK (s.peak > 0.02);
}

TEST_CASE ("Poly mode plays a chord on separate voices", "[voice]")
{
    SynthEngine e;
    e.build (4);
    e.prepare (48000.0, 512, 2);
    e.setVoiceMode (true, 4);
    e.setPanel (basicPatch());
    e.noteOn (60, 1.0f);
    e.noteOn (64, 1.0f);
    e.noteOn (67, 1.0f);
    const auto s = render (e, 24000);
    CHECK (s.finite);
    int active = 0;
    for (int i = 0; i < 4; ++i) if (e.voice (i).isActive()) ++active;
    CHECK (active == 3);
}

TEST_CASE ("CPU budget: one voice at 4x oversampling", "[voice][perf]")
{
    SynthEngine e;
    e.build (5);
    e.prepare (48000.0, 512, 4);
    PanelState p;
    p.emphasis = 0.6;
    p.drift = 1.0;
    e.setPanel (p);
    e.noteOn (48, 1.0f);
    std::vector<float> buf (512);
    const int blocks = 48000 * 2 / 512;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.process (buf.data(), nullptr, 512);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double load = secs / 2.0;
    WARN ("one voice, 4x: " << load * 100.0 << " % of a core");
    CHECK (load < 0.10);
}

// ---------------------------------------------------------------------------
// Real-time safety: no heap allocation while rendering.
namespace
{
std::atomic<bool> trackAllocations { false };
std::atomic<int> allocationCount { 0 };
} // namespace

void* operator new (std::size_t size)
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

TEST_CASE ("Rendering, note events and panel changes do not allocate", "[voice][realtime]")
{
    SynthEngine e;
    e.build (6);
    e.prepare (48000.0, 256, 4);
    PanelState p;
    e.setPanel (p);
    std::vector<float> buf (256), in (256, 0.1f);

    allocationCount = 0;
    trackAllocations = true;
    e.setVoiceMode (true, 6);
    for (int b = 0; b < 200; ++b)
    {
        if (b % 20 == 0) e.noteOn (48 + b / 20, 1.0f);
        if (b % 20 == 10) e.noteOff (48 + b / 20);
        p.cutoff = 0.3 + 0.002 * b;
        p.osc[1].wave = Waveform (b % 6);
        e.setPanel (p);
        e.process (buf.data(), in.data(), 256);
    }
    e.allNotesOff();
    trackAllocations = false;
    CHECK (allocationCount.load() == 0);
}
