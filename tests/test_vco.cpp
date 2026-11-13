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

// Oscillator: calibration accuracy, simulated frequency, waveform sanity, aliasing.

#include "engine/Voice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

using namespace mm;
using Catch::Approx;

namespace
{
/** Measures the frequency of the core ramp by counting resets with
    sub-sample interpolation. */
double measureFrequency (VcoCore& vco, ExpoConverter& ex, double cv, double fs, double seconds)
{
    const double amps = ex.current (cv);
    const int n = int (fs * seconds);
    double lastX = vco.ramp(), firstReset = -1.0, lastReset = -1.0;
    int resets = 0;
    for (int i = 0; i < n; ++i)
    {
        vco.process (amps);
        const double xr = vco.ramp();
        if (xr < lastX - 1.0)
        {
            // reset happened during this sample; estimate the instant from the ramp position
            const double rate = (xr + 1.0); // distance travelled since reset (normalised)
            const double perSample = 2.0 * amps / (vco.params().c * vco.params().swing) / fs;
            const double tEvent = i - rate / perSample;
            if (firstReset < 0.0) firstReset = tEvent; else ++resets;
            lastReset = tEvent;
        }
        lastX = xr;
    }
    return resets > 0 ? resets / ((lastReset - firstReset) / fs) : 0.0;
}
} // namespace

TEST_CASE ("Tuned oscillators: exact on the two tuning notes, a few cents elsewhere over 6 octaves", "[vco]")
{
    // The old board is tuned on two notes only, 440 and 3520 Hz with RANGE at
    // 2' (CV 0 and +3 here), and its high-frequency compensation is a fixed
    // resistor: away from those notes each unit keeps its own error, larger
    // towards the low octaves.
    for (uint64_t seed : { 1ull, 2ull, 3ull, 99ull })
    {
        Voice v;
        v.build (seed);
        for (int osc = 0; osc < 3; ++osc)
        {
            for (int oct = -3; oct <= 3; ++oct)
            {
                const double target = 440.0 * std::pow (2.0, oct);
                const double cents = 1200.0 * std::log2 (v.oscFrequency (osc, oct) / target);
                INFO ("seed " << seed << " osc " << osc << " octave " << oct << " error " << cents << " cents");
                CHECK (std::abs (cents) < (oct == 3 || oct == 0 ? 2.0 : 12.0));
            }
        }
    }
}

TEST_CASE ("Simulated core frequency matches the analytic period", "[vco]")
{
    const double fs = 192000.0;
    Voice v;
    v.build (5);
    v.prepare (fs);
    // Use a dedicated core + expo copy so the voice state is untouched.
    VcoCore core = v.vco (0);
    core.prepare (fs, 1);
    core.setWaveform (Waveform::Sawtooth, false);
    ExpoConverter ex;
    ex.setParams ({});
    ex.setTemperatures (298.15, 298.15, 298.15);
    for (double cv : { -2.0, 0.0, 2.0 })
    {
        const double expected = core.frequencyForCurrent (ex.current (cv));
        const double measured = measureFrequency (core, ex, cv, fs, 1.0);
        INFO ("cv " << cv << " expected " << expected << " measured " << measured);
        CHECK (std::abs (1200.0 * std::log2 (measured / expected)) < 0.5);
    }
}

TEST_CASE ("Waveforms stay within the waveshaper swing and have no DC runaway", "[vco]")
{
    const double fs = 192000.0;
    VcoCore core;
    core.setParams ({});
    core.prepare (fs, 3);
    for (int w = 0; w <= int (Waveform::NarrowRect); ++w)
    {
        core.setWaveform (Waveform (w), false);
        double mn = 1e9, mx = -1e9, sum = 0.0;
        const int n = int (fs * 0.1);
        for (int i = 0; i < n; ++i)
        {
            const double y = core.process (100e-6); // ~2 kHz
            mn = std::fmin (mn, y);
            mx = std::fmax (mx, y);
            sum += y;
        }
        INFO ("waveform " << w << " min " << mn << " max " << mx << " mean " << sum / n);
        // Fig. 9-3's levels: triangle "+1.7 TO -1.7V", sawtooth +/-4 V through
        // R34 / R33 (2.09 V), rectangle "(3.5V)" peak to peak; the shark tooth is
        // their mix. The band-limited edges overshoot a little.
        const auto& pr = core.params();
        const double peak = Waveform (w) == Waveform::Sawtooth ? pr.sawPeak
                          : Waveform (w) == Waveform::Triangle ? pr.triPeak
                          : Waveform (w) == Waveform::SharkTooth ? (1.0 - pr.sharkMix()) * pr.triPeak + pr.sharkMix() * pr.sawPeak
                          : pr.rectPeak;
        CHECK (mx < 1.28 * peak);
        CHECK (mn > -1.28 * peak);
        CHECK (mx - mn > 1.7 * peak);
    }
    CHECK (VcoParams {}.sawPeak == Approx (2.089).margin (0.001));
}

TEST_CASE ("Rectangle widths follow the WAVEFORM switch's bias string", "[vco]")
{
    // -10 V, R32 7.5K, R33 1K, R34 1.5K, ground (Fig. 9-16): 0, -1.5 and -2.5 V
    // against the +/-4 V sawtooth through equal 43K: ngspice on the drawn
    // Schmitt trigger (tools/spice/shapers.cir) gives 50.1, 31.7 and 19.9 %.
    const double fs = 192000.0;
    struct Case { Waveform w; double width; };
    for (const auto& c : { Case { Waveform::Square, 0.501 }, Case { Waveform::WideRect, 0.317 }, Case { Waveform::NarrowRect, 0.199 } })
    {
        VcoCore core;
        core.setParams ({});
        core.prepare (fs, 3);
        core.setWaveform (c.w, false);
        int low = 0;
        const int n = int (fs * 0.5);
        for (int i = 0; i < n; ++i)
            if (core.process (20e-6) < 0.0) ++low; // 500 Hz
        INFO ("waveform " << int (c.w) << " narrow part " << double (low) / n);
        CHECK (double (low) / n == Approx (c.width).margin (0.004));
    }
}

TEST_CASE ("Triangle: the drawn shaper's transfer (ngspice)", "[vco]")
{
    // Q2 saturating then inverting, follower Q1 (Fig. 9-3): ngspice's
    // triangle against the sawtooth (tools/spice/shapers.cir, triangle.dat),
    // less its -0.21 V centre, at normalised ramp positions.
    VcoCore core;
    core.setParams ({});
    core.prepare (96000.0, 1);
    core.setWaveform (Waveform::Triangle, false);
    constexpr std::array<std::pair<double, double>, 9> bench { { { -1.0, 1.737 }, { -0.5, -0.039 }, { -0.1, -1.435 },
                                                               { -0.05, -1.600 }, { 0.0, -1.744 }, { 0.05, -1.655 },
                                                               { 0.1, -1.475 }, { 0.5, -0.036 }, { 1.0, 1.761 } } };
    for (const auto& [r, v] : bench)
    {
        INFO ("ramp " << r << ": " << core.shape (r) << " V, ngspice " << v);
        CHECK (core.shape (r) == Approx (v).margin (0.04));
    }
}

namespace
{
/** Strongest in-band alias (0.02 to 20 kHz) against the fundamental, dB, for an
    oscillator alone at `fs`: harmonics folding back, away from the harmonics. */
double worstAlias (double fs, Waveform w, double f0)
{
    VcoCore core;
    VcoParams p;
    p.thrJitter = 0.0;
    core.setParams (p);
    core.prepare (fs, 1);
    core.setWaveform (w, false);
    const double amps = p.c * p.swing / (1.0 / f0 - p.tReset);
    const int n = 1 << 16;
    std::vector<double> y (static_cast<size_t> (n));
    for (int i = 0; i < 4096; ++i) core.process (amps);
    for (auto& s : y) s = core.process (amps);
    auto dftMag = [&] (double f) {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2 * M_PI * i / (n - 1)); // Hann
            re += win * y[size_t (i)] * std::cos (2 * M_PI * f * i / fs);
            im += win * y[size_t (i)] * std::sin (2 * M_PI * f * i / fs);
        }
        return std::sqrt (re * re + im * im);
    };
    const double fundamental = dftMag (f0);
    double worst = 0.0;
    for (int k = 2; k <= 400; ++k)
    {
        if (k * f0 < fs / 2) continue;
        double fa = std::fmod (k * f0, fs);
        if (fa > fs / 2) fa = fs - fa;
        double r = std::fmod (fa, f0);
        if (r > f0 / 2) r = f0 - r;
        if (fa > 20.0 && fa < 20000.0 && r > 5.0) worst = std::fmax (worst, dftMag (fa));
    }
    return 20.0 * std::log10 (worst / fundamental + 1e-20);
}
} // namespace

TEST_CASE ("Band-limited steps and corners keep aliasing low at high pitch", "[vco]")
{
    // Windowed-sinc BLEP / BLAMP (the polyBLEP it replaced gave -52 dB for the
    // sawtooth and -59 dB for the triangle at 2x, 3.5 kHz).
    struct Case { double fs; Waveform w; double f0, limit; const char* name; };
    for (const auto& c : { Case { 96000.0, Waveform::Sawtooth, 3520.0, -100.0, "sawtooth 2x" },
                           Case { 96000.0, Waveform::Square, 3520.0, -100.0, "square 2x" },
                           Case { 96000.0, Waveform::Triangle, 3520.0, -75.0, "triangle 2x" },
                           Case { 192000.0, Waveform::Sawtooth, 7040.0, -100.0, "sawtooth 4x, 7 kHz" },
                           Case { 192000.0, Waveform::Triangle, 7040.0, -75.0, "triangle 4x, 7 kHz" } })
    {
        const double db = worstAlias (c.fs, c.w, c.f0);
        INFO (c.name << ": worst in-band alias " << db << " dB");
        CHECK (db < c.limit);
    }
}

TEST_CASE ("Shark tooth: the front panel's R030 / R031 against the board's outputs", "[vco]")
{
    // Sawtooth through R34 / R33 (2.25K) and R030 47K, triangle through
    // R28 / R23 (0.69K) and R031 10K (Fig. 9-3, drawing 1448): 17.8 % sawtooth,
    // 8.8K source resistance.
    const VcoParams p;
    CHECK (p.sharkMix() == Approx (0.178).margin (0.002));
    CHECK (p.rSharkOut() == Approx (8.78e3).epsilon (0.01));

    // Loading the VOLUME pot at 10: the output sees the 25K pot in parallel
    // with the 33K summing resistor into the bus (about 14.3K).
    auto gainWith = [] (double rSource) {
        Mixer m;
        m.setParams ({});
        m.setChannel (0, 1.0, true);
        m.setSourceResistance (0, rSource);
        return m.process ({ 1.0, 0.0, 0.0, 0.0, 0.0 });
    };
    const double open = gainWith (0.0);
    const double load = 25.0e3 * 33.47e3 / (25.0e3 + 33.47e3);
    for (double rs : { p.rSharkOut(), p.rSawOut, p.rTriOut })
    {
        INFO ("source " << rs << " ohm");
        CHECK (20.0 * std::log10 (gainWith (rs) / open) == Approx (20.0 * std::log10 (load / (load + rs))).margin (0.1));
    }
    CHECK (20.0 * std::log10 (gainWith (p.rSharkOut()) / open) == Approx (-4.2).margin (0.15));
}
