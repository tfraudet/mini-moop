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


#include "SpiceRunner.h"
#include "circuit/blocks/FilterOutputStage.h"
#include "circuit/blocks/LadderFilter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <sstream>

using namespace mm;
using Catch::Approx;

TEST_CASE ("Filter output stage table matches ngspice", "[filter][spice]")
{
    if (! mmtest::ngspiceAvailable()) SKIP ("ngspice not installed");

    // Fig. 9-11's Q5-Q8 stage as in tools/spice/outstage.cir; the top capacitor's
    // AC voltage swept slowly from -0.3 to +0.3 V (quasi-static: the stage has
    // no capacitor once C9 is replaced by its operating point).
    std::ostringstream net;
    net << ".include " << MINIMOOP_SPICE_DIR << "/models.lib\n"
        << "vp vp 0 10\nvn vn 0 -10\n"
        << "vd d 0 pwl(0 -0.3 1 0.3)\n"
        << "eb8 b8 s8 d 0 0.5\nvb8 s8 0 -0.00574568\neb6 b6 s6 d 0 -0.5\nvb6 s6 0 -0.00978441\n"
        << "q8 out b8 e8 TIS97\nq6 vp b6 e6 TIS97\nr24 e8 vn 220k\nr20 e6 vn 100k\n"
        << "q7 out e8 e57 TIS97\nq5 vp e6 e57 TIS97\nr26 e57 vn 1k\n"
        << "r3 nu out 1k\nr7 nt nu 180\nvnt nt 0 9.739830\n"
        << "rvca out vo 82.82k\nvvo vo 0 3.207904\nremph nu vu 51.33k\nvvu vu 0 8.743435\n"
        << ".options temp=25 tnom=25\n";
    const auto ref = mmtest::runNgspiceTran ("filter_output", net.str(), { "v(out)" }, 1.0e-4, 1.0);
    REQUIRE (ref);
    const double out0 = ref->at (0, 0.5);
    double worst = 0.0;
    for (int i = 0; i <= 600; ++i)
    {
        const double vd = -0.3 + 0.001 * i;
        double slope = 0.0;
        const double model = FilterOutputStage::transfer (vd, slope);
        worst = std::fmax (worst, std::abs (model - (ref->at (0, (vd + 0.3) / 0.6) - out0)));
    }
    INFO ("worst difference " << worst << " V");
    CHECK (worst < 0.02);
}

TEST_CASE ("Filter output stage: gain and asymmetric clipping", "[filter]")
{
    double slope = 0.0;
    CHECK (FilterOutputStage::transfer (0.0, slope) == Approx (0.0).margin (1e-9));
    CHECK (slope == Approx (FilterOutputStage::smallSignalGain).epsilon (0.01));
    CHECK (FilterOutputStage::transfer (-0.3, slope) == Approx (6.43).margin (0.05)); // Q7 cut off
    CHECK (FilterOutputStage::transfer (0.3, slope) == Approx (-3.67).margin (0.05)); // Q7, Q8 saturated
    // Continuous slope (the emphasis loop's Newton solve relies on it)
    for (int i = -140; i < 140; ++i)
    {
        double s0 = 0.0, s1 = 0.0;
        FilterOutputStage::transfer (i * 0.002 - 1.0e-9, s0);
        FilterOutputStage::transfer (i * 0.002 + 1.0e-9, s1);
        CHECK (std::abs (s1 - s0) < 0.5);
    }
}

TEST_CASE ("Filter passband gain matches the board (ngspice: 56.7 at EMPHASIS 0)", "[filter]")
{
    // ngspice, the whole of circuit 6 (Fig. 9-11) with EMPHASIS at 0 (R14 50K)
    // and the cutoff far above: 56.7 from the mixer node to the output at 1 kHz,
    // in phase. Ladder 0.83 (the residual emphasis feedback), output stage 68.3.
    const double fs = 192000.0;
    LadderParams p;
    p.offset.fill (0.0);
    p.noiseRms = 0.0;
    LadderFilter f;
    f.setParams (p);
    f.prepare (fs);
    f.setCutoffCurrent (2.0e-3); // ~40 kHz
    f.setEmphasis (0.0);
    double num = 0.0, den = 0.0;
    for (int n = 0; n < int (0.1 * fs); ++n)
    {
        const double x = 0.002 * std::sin (2.0 * M_PI * 1000.0 * n / fs);
        const double y = f.process (x);
        if (n > int (0.05 * fs)) { num += x * y; den += x * x; }
    }
    const double gain = num / den; // in-phase gain
    INFO ("gain " << gain);
    CHECK (20.0 * std::log10 (gain / 56.7) == Approx (0.0).margin (1.0)); // dB
}

TEST_CASE ("Filter output clips asymmetrically when driven hard", "[filter]")
{
    const double fs = 192000.0;
    LadderParams p;
    p.offset.fill (0.0);
    p.noiseRms = 0.0;
    LadderFilter f;
    f.setParams (p);
    f.prepare (fs);
    f.setCutoffCurrent (2.0e-3);
    f.setEmphasis (0.0);
    double hi = -1e9, lo = 1e9;
    for (int n = 0; n < int (0.1 * fs); ++n)
    {
        const double y = f.process (0.2 * std::sin (2.0 * M_PI * 200.0 * n / fs));
        if (n > int (0.05 * fs)) { hi = std::fmax (hi, y); lo = std::fmin (lo, y); }
    }
    CHECK (hi == Approx (6.4).margin (0.3));
    CHECK (lo == Approx (-3.7).margin (0.3));
}
