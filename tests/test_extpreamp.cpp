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

// External input pre-amplifier and OVERLOAD lamp driver (board 4, circuit 12).

#include "circuit/blocks/ExtPreamp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

using namespace mm;

namespace
{
constexpr double fs = 192000.0;

/** Drives a 1 kHz sine of peak `amp` for `seconds`; returns the peak of the
    pre-amp output over the last 100 ms and leaves the lamp state in `pre`. */
double runSine (ExtPreamp& pre, double amp, double seconds)
{
    const int n = int (seconds * fs), tail = int (0.1 * fs);
    double peakPos = 0.0;
    for (int i = 0; i < n; ++i)
    {
        pre.process (amp * std::sin (2.0 * M_PI * 1000.0 * double (i) / fs));
        if (i >= n - tail) peakPos = std::max (peakPos, pre.output());
    }
    return peakPos;
}
} // namespace

TEST_CASE ("Ext pre-amp: gain of 201 at full volume, rails at +9.8 / -7.7 V", "[extpreamp]")
{
    ExtPreamp pre;
    pre.setParams ({});
    pre.prepare (fs);
    pre.setVolume (1.0);
    CHECK_THAT (runSine (pre, 0.005, 0.5), Catch::Matchers::WithinRel (201.0 * 0.005, 0.02));

    pre.reset();
    runSine (pre, 1.0, 0.3);
    double lo = 0.0, hi = 0.0;
    for (int i = 0; i < int (0.01 * fs); ++i)
    {
        pre.process (std::sin (2.0 * M_PI * 1000.0 * double (i) / fs));
        lo = std::min (lo, pre.output());
        hi = std::max (hi, pre.output());
    }
    CHECK_THAT (hi, Catch::Matchers::WithinAbs (9.8, 0.3));
    CHECK_THAT (lo, Catch::Matchers::WithinAbs (-7.7, 0.3));
}

TEST_CASE ("OVERLOAD lamp lights above about 1.5 V at the pre-amp output (ngspice)", "[extpreamp]")
{
    // ngspice on the drawn lamp driver (tools/spice/overload.cir): a tenth of
    // the lamp's current at a 1.44 V peak, nine tenths at 1.55 V (the service
    // manual, 2.12: "approximately 1.2 volts").
    ExtPreamp pre;
    pre.setParams ({});
    pre.prepare (fs);
    pre.setVolume (1.0);

    runSine (pre, 1.3 / 201.0, 0.5); // 1.3 V peak: below the threshold
    CHECK (pre.lampLevel() < 0.05);

    pre.reset();
    runSine (pre, 1.8 / 201.0, 0.5); // 1.8 V peak: lamp fully lit
    CHECK (pre.lampLevel() > 0.9);

    // Silence: C14 holds for a while, then the lamp goes out.
    for (int i = 0; i < int (1.0 * fs); ++i) pre.process (0.0);
    CHECK (pre.lampLevel() < 0.05);
}

TEST_CASE ("EXTERNAL INPUT VOLUME at 0 mutes the pre-amp", "[extpreamp]")
{
    ExtPreamp pre;
    pre.setParams ({});
    pre.prepare (fs);
    pre.setVolume (0.0);
    CHECK (runSine (pre, 1.0, 0.2) < 1.0e-9);
    CHECK (pre.lampLevel() < 1.0e-6);
}
