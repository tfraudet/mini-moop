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

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

using namespace mm;

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

TEST_CASE ("Calibrated oscillators track 1 V/oct to within a few cents over 6 octaves", "[vco]")
{
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
                // calibration points (A1, A4, A7) within 1 cent; extremes a bit looser,
                // as on a real, well-trimmed instrument.
                CHECK (std::abs (cents) < (std::abs (oct) == 3 || oct == 0 ? 1.0 : 3.0));
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
    ex.calibrate (0.0, 22.0e-6);
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
        CHECK (mx < 3.2);
        CHECK (mn > -3.2);
        CHECK (mx - mn > 3.0);
    }
}

TEST_CASE ("polyBLEP keeps sawtooth aliasing low at high pitch", "[vco]")
{
    // 4x oversampling at 48 kHz host: 192 kHz. A 3.5 kHz sawtooth has harmonics
    // folding above 96 kHz; compare energy at a non-harmonic bin with a harmonic.
    const double fs = 192000.0;
    VcoCore core;
    VcoParams p;
    p.tReset = 0.0;
    p.thrJitter = 0.0;
    core.setParams (p);
    core.prepare (fs, 1);
    core.setWaveform (Waveform::Sawtooth, false);
    const double f0 = 3520.0;
    const double amps = f0 * p.c * p.swing;
    const int n = 1 << 16;
    std::vector<double> y (static_cast<size_t> (n));
    for (auto& s : y) s = core.process (amps);

    auto dftMag = [&] (double f) {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / (n - 1)); // Hann
            re += w * y[size_t (i)] * std::cos (2 * M_PI * f * i / fs);
            im += w * y[size_t (i)] * std::sin (2 * M_PI * f * i / fs);
        }
        return std::sqrt (re * re + im * im);
    };
    const double fundamental = dftMag (f0);
    // Strongest alias inside the audio band: harmonic k folds to |k f0 - fs|.
    double worst = 0.0;
    for (int k = 28; k <= 80; ++k)
    {
        double fa = std::fmod (k * f0, fs);
        if (fa > fs / 2) fa = fs - fa;
        if (fa < 20000.0) worst = std::fmax (worst, dftMag (fa));
    }
    const double db = 20.0 * std::log10 (worst / fundamental);
    INFO ("worst in-band alias: " << db << " dB");
    CHECK (db < -60.0);
}
