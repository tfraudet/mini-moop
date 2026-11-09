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

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

using namespace mm;

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

TEST_CASE ("Noise reaches MODULATION MIX at a level comparable to oscillator 3", "[noise]")
{
    NoiseSource n;
    n.prepare (192000.0, 11);
    n.setWhiteLevel (0.8); // as set by Voice
    double sp = 0.0, sr = 0.0;
    const int count = 192000 * 4;
    for (int i = 0; i < count; ++i)
    {
        double w, p, r;
        n.process (w, p, r);
        sp += p * p;
        sr += r * r;
    }
    const double pinkMod = NoiseSource::modulationGain * std::sqrt (sp / count);
    const double redMod = NoiseSource::modulationGain * std::sqrt (sr / count);
    CHECK (pinkMod > 0.8);  // about 1 V rms against oscillator 3's 2.5 V peak
    CHECK (pinkMod < 1.25);
    CHECK (redMod > 0.3 * pinkMod);
    CHECK (redMod < 0.8 * pinkMod);
}

TEST_CASE ("Main VOLUME is a linear passive divider", "[volume]")
{
    CHECK (OutputVolume::gain (0.0) == 0.0);
    CHECK (std::abs (OutputVolume::gain (1.0) - 1.0) < 1.0e-12);
    // About -6.5 dB at mid travel (an audio taper would give about -20 dB)
    const double mid = 20.0 * std::log10 (OutputVolume::gain (0.5));
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
    CHECK (rms (true, 1.0) > 0.1);
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
