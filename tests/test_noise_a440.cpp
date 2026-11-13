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

// Noise colours (white / pink / red) and the A-440 reference path.

#include "circuit/blocks/ModulationMix.h"
#include "circuit/blocks/NoiseSource.h"
#include "circuit/blocks/OutputVolume.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

using namespace mm;
using Catch::Approx;

TEST_CASE ("Red noise is darker than pink, pink darker than white", "[noise]")
{
    NoiseSource n;
    n.prepare (48000.0, 7);
    n.setWhiteLevel (1.0);
    // Ratio of the RMS of the first difference (high frequencies) to the RMS.
    double sw[2] {}, sp[2] {}, sr[2] {}, pw = 0, pp = 0, pr = 0;
    for (int i = 0; i < 480000; ++i)
    {
        double w, p, r;
        n.process (w, p, r);
        sw[0] += w * w; sw[1] += (w - pw) * (w - pw); pw = w;
        sp[0] += p * p; sp[1] += (p - pp) * (p - pp); pp = p;
        sr[0] += r * r; sr[1] += (r - pr) * (r - pr); pr = r;
    }
    const double hfWhite = sw[1] / sw[0], hfPink = sp[1] / sp[0], hfRed = sr[1] / sr[0];
    CHECK (hfPink < hfWhite);
    CHECK (hfRed < 0.2 * hfPink);
}

TEST_CASE ("The noise outputs' levels do not depend on the sample rate", "[noise]")
{
    // White: the level set, in the amplifier's 37 kHz bandwidth, less what lies
    // above Nyquist at the lower rate (its density is kept, not its total).
    // Pink and red: what board 3's networks make of it, the same at both rates.
    double whiteAt[2] {}, pinkAt[2] {}, redAt[2] {};
    int k = 0;
    for (double fs : { 96000.0, 384000.0 })
    {
        NoiseSource n;
        n.prepare (fs, 11);
        n.setWhiteLevel (0.1);
        double sw = 0.0, sp = 0.0, sr = 0.0;
        const int count = int (fs * 4.0);
        for (int i = 0; i < count; ++i)
        {
            double w, p, r;
            n.process (w, p, r);
            sw += w * w;
            sp += p * p;
            sr += r * r;
        }
        INFO ("fs " << fs << ": white " << std::sqrt (sw / count) << ", pink " << std::sqrt (sp / count)
              << ", red " << std::sqrt (sr / count));
        whiteAt[k] = std::sqrt (sw / count);
        pinkAt[k] = std::sqrt (sp / count);
        redAt[k] = std::sqrt (sr / count);
        ++k;
    }
    CHECK (whiteAt[1] > 0.09);
    CHECK (whiteAt[1] < 0.11);
    CHECK (whiteAt[0] > 0.07);
    CHECK (whiteAt[0] < whiteAt[1]);
    CHECK (pinkAt[1] / pinkAt[0] > 0.9);
    CHECK (pinkAt[1] / pinkAt[0] < 1.1);
    CHECK (redAt[1] / redAt[0] > 0.85);
    CHECK (redAt[1] / redAt[0] < 1.15);
    CHECK (redAt[0] < pinkAt[0]);
}

TEST_CASE ("Main VOLUME is a linear passive divider", "[volume]")
{
    CHECK (OutputVolume::gain (0.0) == 0.0);
    // Fully clockwise: R75 1K into the pot's 5K beside the 10K load
    CHECK (std::abs (OutputVolume::gain (1.0) - (10.0 / 3.0) / (1.0 + 10.0 / 3.0)) < 1.0e-9);
    // About -6.5 dB at mid travel (an audio taper would give about -20 dB)
    const double mid = 20.0 * std::log10 (OutputVolume::gain (0.5) / OutputVolume::gain (1.0));
    CHECK (mid > -7.5);
    CHECK (mid < -5.5);
    for (int i = 0; i < 10; ++i)
        CHECK (OutputVolume::gain (0.1 * (i + 1)) > OutputVolume::gain (0.1 * i));
}

TEST_CASE ("A-440 follows the output VOLUME and ON switch", "[a440]")
{
    auto rms = [] (bool mainOn, double volume) {
        SynthEngine e;
        e.build (1);
        e.prepare (48000.0, 512, 2);
        PanelState p;
        p.a440 = true;
        p.mainOn = mainOn;
        p.volume = volume;
        e.setPanel (p);
        std::vector<float> out (512);
        double s = 0.0;
        for (int b = 0; b < 40; ++b)
        {
            e.process (out.data(), nullptr, 512);
            if (b >= 20) for (float v : out) s += double (v) * double (v);
        }
        return std::sqrt (s / (20.0 * 512.0));
    };
    CHECK (rms (true, 1.0) > 0.05); // 0.436 V peak at the jack, times the engine's 0.35
    CHECK (rms (true, 0.5) < 0.5 * rms (true, 1.0));
    CHECK (rms (false, 1.0) < 1.0e-6);
}

TEST_CASE ("MODULATION MIX: R3 network with grounded wiper", "[modmix]")
{
    auto [o0, n0] = ModulationMix::gains (0.0);
    auto [o1, n1] = ModulationMix::gains (1.0);
    CHECK (std::abs (o0 - 1.0) < 1.0e-9);
    CHECK (n0 < 1.0e-3);   // wiper shorts the noise end
    CHECK (o1 < 1.0e-3);   // wiper shorts the oscillator 3 end
    CHECK (std::abs (n1 - 1.0) < 1.0e-9);
    // Symmetric, and both sources well above half level at mid travel
    auto [om, nm] = ModulationMix::gains (0.5);
    CHECK (std::abs (om - nm) < 1.0e-9);
    CHECK (om > 0.7);
    CHECK (om < 0.8);
    for (int i = 0; i < 10; ++i)
    {
        CHECK (ModulationMix::gains (0.1 * (i + 1)).first < ModulationMix::gains (0.1 * i).first);
        CHECK (ModulationMix::gains (0.1 * (i + 1)).second > ModulationMix::gains (0.1 * i).second);
    }
}

TEST_CASE ("Modulation amplifier: gain and the floor of its one-way output (ngspice)", "[modmix]")
{
    // Fig. 9-8 circuit 15 against ngspice on the drawn circuit
    // (tools/spice/modulation.cir): oscillator 3 alone (MODULATION MIX at 0),
    // the line loaded by the three MOD inputs and R52, for three positions
    // of the MODULATION wheel. Columns: the wheel (ohm), then the output for
    // +1.5 V, +2 V and -2 V at oscillator 3's input.
    struct Row { double wheel, at15, at2, atMinus2, floor; };
    const Row bench[] = { { 1200.0, -1.196, -1.595, 1.702, -1.692 },
                          { 600.0, -1.183, -1.336, 1.697, -1.336 },
                          { 200.0, -1.056, -1.055, 1.691, -1.056 } };
    const double gain = ModulationMix::amplifier (0.0).first;
    CHECK (gain == Approx (0.829).epsilon (0.005));
    for (const auto& r : bench)
    {
        const double rLine = 1.0 / (1.0 / r.wheel + 3.0 / 51.1e3 + 1.0 / 33.0e3);
        const double floor = ModulationMix::outputFloor (rLine);
        INFO ("wheel " << r.wheel << " ohm: floor " << floor);
        CHECK (floor == Approx (r.floor).margin (0.015));
        constexpr double off = ModulationMix::outputOffset;
        CHECK (ModulationMix::limit (-gain * 1.5 + off, floor) == Approx (r.at15).margin (0.02));
        CHECK (ModulationMix::limit (-gain * 2.0 + off, floor) == Approx (r.at2).margin (0.02));
        CHECK (ModulationMix::limit (gain * 2.0 + off, floor) == Approx (r.atMinus2).margin (0.02));
    }
}
