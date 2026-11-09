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

// Ladder filter: small-signal response, self-oscillation, and comparison of
// the reduced (per-stage Ebers-Moll) model against a full transistor-level
// ladder simulated by ngspice.

#include "SpiceRunner.h"
#include "circuit/blocks/LadderFilter.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <sstream>

using namespace mm;

namespace
{
LadderParams idealParams()
{
    LadderParams p;
    p.offset.fill (0.0);
    p.beta = 1e9;
    p.noiseRms = 0.0;
    return p;
}

/** Amplitude of the steady-state response to a small sine (in stage-4 volts). */
double sineGain (LadderFilter& f, double fs, double freq, double amp)
{
    f.reset();
    const int settle = int (fs * 0.2), measure = int (fs * 0.2);
    double peak = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        f.process (amp * std::sin (2 * M_PI * freq * i / fs));
        if (i >= settle) peak = std::fmax (peak, std::abs (f.stageVoltage (3)));
    }
    return peak;
}
} // namespace

TEST_CASE ("Ladder: -12 dB at the small-signal cutoff without emphasis", "[ladder]")
{
    const double fs = 192000.0;
    LadderFilter f;
    auto p = idealParams();
    p.kMax = 0.0; // no feedback at all (EMPHASIS at 0 still leaks a little through R14)
    f.setParams (p);
    f.prepare (fs);
    f.setTemperature (300.15);
    const double iTail = 100e-6;
    f.setCutoffCurrent (iTail);
    f.setEmphasis (0.0);
    const double fc = f.nominalCutoffForCurrent (iTail);

    const double vin = 0.1; // 0.1 V * 0.012 = 1.2 mV at the input pair: linear region
    const double inDiff = vin * p.inputGain;
    const double gDc = sineGain (f, fs, 20.0, vin) / inDiff;
    const double gFc = sineGain (f, fs, fc, vin) / inDiff;
    CHECK (std::abs (gDc - 1.0) < 0.02);
    CHECK (std::abs (20 * std::log10 (gFc) + 12.04) < 0.5);
}

TEST_CASE ("Ladder: self-oscillates near cutoff at full emphasis", "[ladder]")
{
    const double fs = 192000.0;
    LadderFilter f;
    auto p = idealParams();
    p.noiseRms = 1e-6;
    f.setParams (p);
    f.prepare (fs, 7);
    f.setTemperature (300.15);
    const double iTail = 50e-6;
    f.setCutoffCurrent (iTail);
    f.setEmphasis (1.0);
    const double fc = f.nominalCutoffForCurrent (iTail);

    const int n = int (fs * 0.5);
    int crossings = 0;
    double prev = 0.0, peak = 0.0;
    for (int i = 0; i < n; ++i)
    {
        f.process (0.0);
        const double y = f.stageVoltage (3);
        if (i > n / 2)
        {
            if (prev < 0.0 && y >= 0.0) ++crossings;
            peak = std::fmax (peak, std::abs (y));
        }
        prev = y;
    }
    const double fosc = crossings / (0.5 * n / fs);
    CHECK (peak > 0.005);                    // sustained oscillation (~9 mV across C4 at k = 4.6)
    CHECK (std::abs (fosc / fc - 1.0) < 0.1); // oscillates at the cutoff frequency
}

TEST_CASE ("Ladder reduced model matches transistor-level ngspice ladder", "[ladder][spice]")
{
    if (! mmtest::ngspiceAvailable()) SKIP ("ngspice not installed");

    const double fs = 192000.0, h = 1.0 / fs, stop = 0.02;
    const double C = 0.068e-6, iTail = 100e-6, k = 2.5, bf = 220.0;
    const double amp = 0.04, freq = 440.0; // 40 mV differential: well into the tanh region

    // Transistor ladder: input pair Q1/Q2, four ladder pairs, capacitor across
    // each level, collector loads at the top. Q2's base carries the emphasis
    // feedback k*u4 (ideal VCVS, the filter's feedback amp is modelled separately).
    std::ostringstream net;
    net << "VCC vcc 0 15\nVEE vee 0 -15\n"
        << "VIN b1 0 SIN(0 " << amp << " " << freq << ")\n"
        << "EFB b2 0 r4 l4 " << k << "\n" // V(l)-V(r) = -u_n (pulled-down side)
        << "IT e0 vee " << iTail << "\n"
        << "Q1 l1 b1 e0 QN\nQ2 r1 b2 e0 QN\n";
    for (int n = 1; n <= 4; ++n)
    {
        net << "VB" << n << " bias" << n << " 0 " << (1.0 + 1.0 * n) << "\n";
        net << "C" << n << " l" << n << " r" << n << " " << C << "\n";
        net << "QL" << n << " l" << (n + 1) << " bias" << n << " l" << n << " QN\n";
        net << "QR" << n << " r" << (n + 1) << " bias" << n << " r" << n << " QN\n";
    }
    net << "RL l5 vcc 2k\nRR r5 vcc 2k\n";
    net << ".model QN NPN(IS=6.734f BF=" << bf << " BR=0.7371 VAF=74.03)\n";
    const auto ref = mmtest::runNgspiceTran ("ladder", net.str(), { "v(r4)-v(l4)" }, h, stop);
    REQUIRE (ref);

    LadderParams p;
    p.offset.fill (0.0);
    p.c.fill (C);
    p.beta = bf;
    p.inputGain = 1.0;
    p.kMax = k;
    p.noiseRms = 0.0;
    LadderFilter f;
    f.setParams (p);
    f.prepare (fs);
    f.setTemperature (300.15);
    f.setCutoffCurrent (iTail);
    f.setEmphasis (1.0);

    std::vector<double> sim, r;
    const int n = int (std::round (stop / h));
    for (int i = 1; i <= n; ++i)
    {
        const double t = i * h;
        f.process (amp * std::sin (2 * M_PI * freq * t));
        // Model u_4 is defined with the same orientation as V(l4)-V(r4) up to sign.
        sim.push_back (f.stageVoltage (3));
        r.push_back (ref->at (0, t));
    }
    // Determine sign convention once, then compare (skip the first 5 ms of DC settling).
    double dot = 0.0;
    for (size_t i = 0; i < sim.size(); ++i) dot += sim[i] * r[i];
    const double sgn = dot >= 0.0 ? 1.0 : -1.0;
    std::vector<double> a, b;
    for (size_t i = size_t (fs * 0.005); i < sim.size(); ++i) { a.push_back (sgn * sim[i]); b.push_back (r[i]); }
    const double err = mmtest::relativeRmsError (a, b);
    INFO ("relative RMS error vs ngspice: " << err);
    CHECK (err < 0.05);
}

TEST_CASE ("EMPHASIS: reverse-audio law, self-oscillation from about 7.5", "[ladder]")
{
    const LadderParams p;
    auto k = [&p] (double dial) { return p.kMax * LadderFilter::emphasisLaw (p, dial / 10.0); };
    // The ladder self-oscillates for k above about 4.0
    CHECK (k (7.0) < 4.0);
    CHECK (k (8.0) > 4.0);
    // Monotonic, small residual feedback fully counter-clockwise
    for (int i = 0; i < 100; ++i)
        CHECK (k (0.1 * (i + 1)) > k (0.1 * i));
    CHECK (k (0.0) < 0.5);
}
