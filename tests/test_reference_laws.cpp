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


// Panel laws and values measured on the CA-72 project's hardware reference
// (https://github.com/idlefoundry/ca-72, docs/calibration), which replace
// values that were assumed: LO's range, the FREQUENCY knobs, GLIDE, CUTOFF,
// and board 3's pink network.

#include "circuit/blocks/Glide.h"
#include "circuit/blocks/NoiseSource.h"
#include "engine/PanelLaws.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace mm;
using Catch::Approx;

namespace
{
/** Fundamental (Hz) of a held note, from rising zero crossings after 1 s. */
double heldFrequency (PanelState p, int note, double seconds, uint64_t unit = 1)
{
    SynthEngine e;
    e.build (unit);
    e.prepare (48000.0, 256, 4);
    p.drift = 0.0;
    e.setPanel (p);
    std::vector<float> y (size_t (48000.0 * (1.0 + seconds)));
    e.noteOn (note, 1.0f);
    for (size_t i = 0; i < y.size(); i += 256)
        e.process (y.data() + i, nullptr, int (std::min<size_t> (256, y.size() - i)));
    int first = -1, last = -1, n = 0;
    for (size_t i = 48000; i + 1 < y.size(); ++i)
        if (y[i] <= 0.0f && y[i + 1] > 0.0f)
        {
            if (first < 0) first = int (i);
            last = int (i);
            ++n;
        }
    return n > 1 ? (n - 1) * 48000.0 / double (last - first) : 0.0;
}

PanelState oneOscillator (int osc)
{
    PanelState p;
    for (int i = 0; i < 3; ++i)
    {
        p.osc[size_t (i)].on = i == osc;
        p.osc[size_t (i)].wave = Waveform::Triangle;
    }
    p.osc3Control = true;
    p.cutoff = 1.0;
    p.contourAmount = 0.0;
    p.lSustain = 1.0;
    p.volume = 1.0;
    return p;
}
} // namespace

TEST_CASE ("LO is 6.74 octaves below 8'", "[reference]")
{
    PanelState p = oneOscillator (0);
    const double f8 = heldFrequency (p, 93, 1.0);   // A6
    p.osc[0].range = Range::Lo;
    const double fLo = heldFrequency (p, 93, 2.0);
    CHECK (std::log2 (f8 / fLo) == Approx (6.74).margin (0.02));
}

TEST_CASE ("FREQUENCY knobs: the reference's semitones at -5 and +5", "[reference]")
{
    struct Mark { int osc; double mark, semitones; };
    for (const auto& m : { Mark { 1, -5.0, -6.48 }, Mark { 1, 5.0, 6.61 }, Mark { 2, -5.0, -6.73 }, Mark { 2, 5.0, 6.92 } })
    {
        INFO ("oscillator " << m.osc + 1 << ", mark " << m.mark);
        PanelState p = oneOscillator (m.osc);
        const double f0 = heldFrequency (p, 57, 1.0);
        p.osc[size_t (m.osc)].frequency = 0.5 + m.mark / 15.0;
        const double f = heldFrequency (p, 57, 1.0);
        CHECK (12.0 * std::log2 (f / f0) == Approx (m.semitones).margin (0.05));
    }
}

TEST_CASE ("GLIDE slides at the reference's rates", "[reference]")
{
    // 20-80 % of an octave, octaves a second, up and down (session J).
    struct Rate { double pos, up, down; };
    const double fs = 192000.0;
    for (const auto& r : { Rate { 0.25, 61.0, 22.0 }, Rate { 0.5, 29.0, 18.0 }, Rate { 0.75, 3.8, 2.1 }, Rate { 1.0, 1.6, 0.9 } })
    {
        INFO ("GLIDE " << r.pos * 10.0);
        for (int dir : { 1, -1 })
        {
            Glide g;
            g.setParams ({});
            g.prepare (fs);
            g.setEnabled (true);
            g.setTime (r.pos);
            const double from = dir > 0 ? 0.0 : 1.0, to = 1.0 - from;
            g.reset (from);
            int n = 0, n20 = -1, n80 = -1;
            while (n80 < 0 && n < int (10.0 * fs))
            {
                const double v = g.process (to);
                const double done = (v - from) / (to - from);
                if (n20 < 0 && done >= 0.2) n20 = n;
                if (done >= 0.8) n80 = n;
                ++n;
            }
            const double rate = 0.6 / (double (n80 - n20) / fs);
            CHECK (rate == Approx (dir > 0 ? r.up : r.down).epsilon (0.3));
        }
    }
}

TEST_CASE ("CUTOFF: the service manual's law, 440 Hz at -1 and 0.9 octave a mark", "[reference]")
{
    // Filter Range (Table 5-4): EMPHASIS 10, every source off, KEYBOARD
    // CONTROL off; the filter sings at 440 Hz at -1. Specifications: 9 octaves
    // over the dial. Averaged over four units (their parts' tolerances spread
    // them over about +-1.5 semitones).
    auto sings = [] (double mark, uint64_t unit) {
        PanelState p;
        for (auto& o : p.osc) o.on = false;
        p.cutoff = (mark + 5.0) / 10.0;
        p.emphasis = 1.0;
        p.contourAmount = 0.0;
        p.lSustain = 1.0;
        p.volume = 1.0;
        return heldFrequency (p, 60, 1.0, unit);
    };
    struct Mark { double mark, margin; };
    for (const auto& m : { Mark { -1.0, 0.25 }, Mark { -3.0, 1.0 }, Mark { 1.0, 1.0 }, Mark { 3.0, 1.0 } })
    {
        INFO ("mark " << m.mark);
        const double hz = laws::cutoffHz ((m.mark + 5.0) / 10.0);
        double sum = 0.0;
        for (uint64_t unit = 1; unit <= 4; ++unit)
            sum += 12.0 * std::log2 (sings (m.mark, unit) / hz);
        CHECK (sum / 4.0 == Approx (0.0).margin (m.margin)); // semitones
    }
    // Filter Cutoff Adjustment (5.13): at least 16 kHz fully clockwise, under
    // 300 Hz fully anticlockwise, on every unit.
    for (uint64_t unit = 1; unit <= 4; ++unit)
    {
        INFO ("unit " << unit);
        CHECK (sings (5.0, unit) >= 16000.0);
        CHECK (sings (-5.0, unit) < 300.0);
    }
}

TEST_CASE ("Pink noise against white follows board 3's network (ngspice)", "[reference]")
{
    // ngspice, Fig. 9-8 with C2 .022 uF and R13 510: pink at Q3's collector
    // against white at Q4's emitter.
    struct Point { double hz, dB; };
    const Point ref[] = { { 100, 19.52 }, { 200, 16.08 }, { 500, 11.83 }, { 1000, 10.09 }, { 2000, 8.46 },
                          { 5000, 4.33 }, { 10000, 0.43 } };
    const double fs = 192000.0;
    NoiseSource n;
    n.prepare (fs, 11);
    n.setWhiteLevel (1.0);
    const int block = 8192, blocks = 96;
    std::vector<double> w (size_t (block * blocks)), p (w.size());
    for (int i = 0; i < int (fs); ++i) { double a, b, c; n.process (a, b, c); }
    for (size_t i = 0; i < w.size(); ++i) { double c; n.process (w[i], p[i], c); }
    for (const auto& r : ref)
    {
        double pw = 0.0, pp = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            double rw = 0, iw = 0, rp = 0, ip = 0;
            for (int i = 0; i < block; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / block);
                const double c = std::cos (2.0 * M_PI * r.hz * i / fs), s = std::sin (2.0 * M_PI * r.hz * i / fs);
                const size_t k = size_t (b * block + i);
                rw += win * w[k] * c; iw += win * w[k] * s;
                rp += win * p[k] * c; ip += win * p[k] * s;
            }
            pw += rw * rw + iw * iw;
            pp += rp * rp + ip * ip;
        }
        INFO (r.hz << " Hz");
        CHECK (10.0 * std::log10 (pp / pw) == Approx (r.dB).margin (0.8));
    }
}

TEST_CASE ("KEYBOARD CONTROL: the filter follows the keys at 1/3, 2/3 and 1 octave an octave", "[voice]")
{
    // The filter singing alone (EMPHASIS 10, sources off), CUTOFF at its
    // centre, C3 then C4: KBD 1, KBD 2 and both (Moog specifications).
    struct Case { bool kbd1, kbd2; double octaves; };
    for (const auto& c : { Case { true, false, 1.0 / 3.0 }, Case { false, true, 2.0 / 3.0 }, Case { true, true, 1.0 } })
    {
        INFO ("KBD 1 " << c.kbd1 << ", KBD 2 " << c.kbd2);
        PanelState p;
        for (auto& o : p.osc) o.on = false;
        p.cutoff = 0.5;
        p.emphasis = 1.0;
        p.contourAmount = 0.0;
        p.lSustain = 1.0;
        p.volume = 1.0;
        p.kbd1 = c.kbd1;
        p.kbd2 = c.kbd2;
        const double low = heldFrequency (p, 48, 1.0), high = heldFrequency (p, 60, 1.0);
        CHECK (std::log2 (high / low) == Approx (c.octaves).margin (0.03));
    }
}

namespace
{
/** Pitch swing (octaves, peak to peak) of a held note's output, from the
    intervals between rising zero crossings: 2nd to 98th percentile. */
double pitchSwing (PanelState p, int note, double seconds)
{
    SynthEngine e;
    e.build (1);
    e.prepare (48000.0, 256, 4);
    p.drift = 0.0;
    e.setPanel (p);
    std::vector<float> y (size_t (48000.0 * (1.0 + seconds)));
    e.noteOn (note, 1.0f);
    for (size_t i = 0; i < y.size(); i += 256)
        e.process (y.data() + i, nullptr, int (std::min<size_t> (256, y.size() - i)));
    std::vector<double> f;
    double last = -1.0;
    for (size_t i = 48000; i + 1 < y.size(); ++i)
        if (y[i] <= 0.0f && y[i + 1] > 0.0f)
        {
            const double t = double (i) + y[i] / (y[i] - y[i + 1]); // sub-sample crossing
            if (last >= 0.0) f.push_back (48000.0 / (t - last));
            last = t;
        }
    std::sort (f.begin(), f.end());
    return std::log2 (f[size_t (0.98 * double (f.size() - 1))] / f[size_t (0.02 * double (f.size() - 1))]);
}
} // namespace

TEST_CASE ("Modulation line: depth and MOD. wheel law from the circuit", "[voice]")
{
    // Oscillator 3's triangle on LO (about 2 Hz) through MODULATION MIX into
    // the line (Fig. 9-8, 9-12). Fully forward the wheel is 1.2K against R57
    // 1K: the oscillators swing +-0.74 octave (ngspice: +0.75 / -0.71 V). At
    // half way the wheel is 0.39K: about half the depth.
    PanelState base;
    base.osc[2].range = Range::Lo;
    base.osc[2].wave = Waveform::Triangle;
    base.osc[2].on = false;
    base.modMix = 0.0;
    base.contourAmount = 0.0;
    base.lSustain = 1.0;
    base.volume = 1.0;

    PanelState osc = base;
    osc.osc[0].wave = Waveform::Triangle;
    osc.osc[1].on = false;
    osc.cutoff = 1.0;
    osc.oscModulation = true;
    osc.modWheel = 1.0;
    const double full = pitchSwing (osc, 57, 3.0);
    osc.modWheel = 0.5;
    const double half = pitchSwing (osc, 57, 3.0);

    PanelState filter = base;
    filter.osc[0].on = false;
    filter.osc[1].on = false;
    filter.cutoff = 0.5;
    filter.emphasis = 1.0;
    filter.filterModulation = true;
    filter.modWheel = 0.5;
    const double filterHalf = pitchSwing (filter, 57, 3.0);

    INFO ("oscillators: " << full << " oct at full, " << half << " at half; filter " << filterHalf << " at half");
    CHECK (full == Approx (2.0 * 0.74).epsilon (0.1));
    CHECK (half / full == Approx (0.52).epsilon (0.1));
    // FILTER MODULATION: 100K / 33K as deep as the oscillators' (the loads differ a little)
    CHECK (filterHalf / half == Approx (100.0 / 33.0).epsilon (0.1));
}

TEST_CASE ("Modulation polarity: oscillator 3's rising sawtooth lowers the pitch", "[voice]")
{
    // The modulation amplifier inverts (Fig. 9-8) and the line enters the
    // oscillators' summer as the keyboard does (Fig. 9-3): during oscillator
    // 3's rising ramp the pitch falls, and it jumps up at each reset.
    PanelState p;
    p.osc[0].wave = Waveform::Triangle;
    p.osc[1].on = false;
    p.osc[2].on = false;
    p.osc[2].range = Range::Lo;
    p.osc[2].wave = Waveform::Sawtooth;
    p.modMix = 0.0;
    p.modWheel = 1.0;
    p.oscModulation = true;
    p.cutoff = 1.0;
    p.contourAmount = 0.0;
    p.lSustain = 1.0;
    p.drift = 0.0;
    SynthEngine e;
    e.build (1);
    e.prepare (48000.0, 256, 4);
    e.setPanel (p);
    std::vector<float> y (size_t (48000.0 * 4.0));
    e.noteOn (57, 1.0f);
    for (size_t i = 0; i < y.size(); i += 256)
        e.process (y.data() + i, nullptr, 256);
    std::vector<double> f;
    double last = -1.0;
    for (size_t i = 48000; i + 1 < y.size(); ++i)
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
