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


#include "circuit/blocks/Vca.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace mm;
using Catch::Approx;

TEST_CASE ("VCA gain follows the loudness contour as the circuit does (ngspice)", "[vca]")
{
    // ngspice, board 4 circuit 7 (Fig. 9-11, R43 270K, 25 C): small-signal gain
    // at 1 kHz from the filter output to the main output, against the loudness
    // contour's voltage, relative to the gain at 5 V.
    struct Point { double volts, gain; };
    const Point ngspice[] = { { 0.3, 0.0062 }, { 0.4, 0.0236 }, { 0.5, 0.0457 }, { 0.75, 0.1064 },
                              { 1.0, 0.1698 }, { 1.5, 0.2993 }, { 2.0, 0.4304 }, { 2.5, 0.5623 },
                              { 3.0, 0.6946 }, { 3.5, 0.8274 }, { 4.0, 0.9602 }, { 4.5, 1.0054 },
                              { 5.0, 1.0 }, { 5.5, 0.9917 }, { 6.0, 0.9823 }, { 7.0, 0.9624 },
                              { 8.0, 0.9416 }, { 9.0, 0.9203 } };
    Vca v;
    v.setParams ({});
    const double at5 = v.tailCurrent (5.0);
    for (const auto& p : ngspice)
    {
        INFO (p.volts << " V");
        const double g = v.tailCurrent (p.volts) / at5;
        if (p.gain > 0.03)
            CHECK (20.0 * std::log10 (g / p.gain) == Approx (0.0).margin (0.25)); // dB
        else
            CHECK (g == Approx (p.gain).margin (0.006));
    }
    // Closed at the contour's rest (-0.5 V)
    CHECK (v.tailCurrent (-0.5) < 1.0e-9);
}

TEST_CASE ("VCA small-signal gain is unity at a 5 V contour", "[vca]")
{
    const double fs = 192000.0;
    // Feedthrough adds a DC term; measure the swing instead of the peak.
    auto swing = [fs] (double contour) {
        Vca v;
        v.setParams ({});
        v.prepare (fs);
        double lo = 1e9, hi = -1e9;
        for (int n = 0; n < int (0.05 * fs); ++n)
        {
            const double y = v.process (0.1 * std::sin (2.0 * M_PI * 220.0 * n / fs), contour);
            if (n > int (0.02 * fs)) { lo = std::fmin (lo, y); hi = std::fmax (hi, y); }
        }
        return 0.5 * (hi - lo);
    };
    CHECK (swing (5.0) == Approx (0.1).epsilon (0.05));
    CHECK (swing (4.5) == Approx (0.1).epsilon (0.05)); // saturated: the top of the decay plays at full gain
    CHECK (swing (2.0) < 0.05);
    CHECK (swing (-0.5) < 0.0001);
}
