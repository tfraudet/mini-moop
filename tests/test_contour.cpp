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

// Contour generator: the simulated times must read like the panel legend of
// ATTACK TIME and DECAY TIME (10, 200, 600 ms, 1, 5, 10 s).

#include "circuit/blocks/ContourGen.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace mm;

namespace
{
constexpr double fs = 48000.0;

/** Time (s) from gate on to the end of the attack (peak) at a dial position. */
double attackTime (double dial)
{
    ContourGen c;
    c.setParams ({});
    c.prepare (fs);
    c.setAttack (dial);
    c.gateOn();
    int n = 0;
    while (c.currentStage() == ContourGen::Stage::Attack && n < int (60.0 * fs)) { c.process(); ++n; }
    return double (n) / fs;
}

struct Mark { double dial, seconds; };
const Mark legend[] = { { 0.08, 0.010 }, { 0.165, 0.2 }, { 0.385, 0.6 }, { 0.615, 1.0 }, { 0.735, 5.0 }, { 0.89, 10.0 } };
} // namespace

TEST_CASE ("ATTACK TIME matches the panel legend", "[contour]")
{
    for (const auto& m : legend)
    {
        INFO ("dial " << m.dial << ", legend " << m.seconds << " s");
        const double t = attackTime (m.dial);
        CHECK (t > m.seconds * 0.75);
        CHECK (t < m.seconds * 1.25);
    }
    // Spec: about 1 ms fully counter-clockwise, 10 s fully clockwise
    CHECK (attackTime (0.0) < 0.002);
    CHECK (attackTime (1.0) > 9.0);
}

TEST_CASE ("DECAY TIME legend reads the decay time constant", "[contour]")
{
    for (const auto& m : legend)
    {
        INFO ("dial " << m.dial << ", legend " << m.seconds << " s");
        ContourGen c;
        c.setParams ({});
        c.prepare (fs);
        c.setDecay (m.dial);
        CHECK (c.decayTau() > m.seconds * 0.75);
        CHECK (c.decayTau() < m.seconds * 1.25);
    }
    // Spec: decay from about 4 ms to 35 s (read as ~3 time constants)
    ContourGen c;
    c.setParams ({});
    c.prepare (fs);
    c.setDecay (0.0);
    CHECK (3.0 * c.decayTau() < 0.006);
    c.setDecay (1.0);
    CHECK (3.5 * c.decayTau() > 30.0);
}
