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

// Contour generator: ATTACK and DECAY follow the pot laws measured on the
// CA-72 project's hardware reference; levels follow the circuit (ngspice).

#include "circuit/blocks/ContourGen.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace mm;
using Catch::Approx;

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

/** 10-90 % of the attack (the output from rest to the peak), s. */
double attackRise (const ContourParams& p, double dial)
{
    ContourGen c;
    c.setParams (p);
    c.prepare (fs);
    c.reset();
    c.setAttack (dial);
    const double rest = c.output(), span = p.peakOutput() - rest;
    c.gateOn();
    int n = 0, n10 = -1;
    while (c.currentStage() == ContourGen::Stage::Attack && n < int (60.0 * fs))
    {
        const double v = c.process();
        ++n;
        if (n10 < 0 && v >= rest + 0.1 * span) n10 = n;
        if (v >= rest + 0.9 * span) break;
    }
    return double (n - n10) / fs;
}
} // namespace

TEST_CASE ("ATTACK and DECAY follow the hardware reference's pot laws", "[contour]")
{
    // CA-72's measured resistances at the reference's dial marks, over 0.89
    // (its capacitor's dielectric absorption, absent here).
    struct Point { double dial, attackL, decayL, attackF, decayF; };
    const Point law[] = { { 0.2, 14.13e3, 13.00e3, 13.56e3, 15.94e3 }, { 0.5, 41.85e3, 37.04e3, 36.02e3, 43.46e3 },
                          { 0.7, 267.0e3, 206.4e3, 225.9e3, 236.8e3 }, { 1.0, 982.6e3, 755.4e3, 946.8e3, 768.2e3 } };
    const auto l = ContourParams::loudness(), f = ContourParams::filter();
    for (const auto& pt : law)
    {
        INFO ("dial " << pt.dial);
        CHECK (ContourGen::attackPot (l, pt.dial) == Approx (pt.attackL / 0.89));
        CHECK (ContourGen::decayPot (l, pt.dial) == Approx (pt.decayL / 0.89));
        CHECK (ContourGen::attackPot (f, pt.dial) == Approx (pt.attackF / 0.89));
        CHECK (ContourGen::decayPot (f, pt.dial) == Approx (pt.decayF / 0.89));
        // The attack's 10-90 % time is about the network's time constant
        ContourGen c;
        c.setParams (l);
        c.prepare (fs);
        c.setAttack (pt.dial);
        CHECK (attackRise (l, pt.dial) == Approx (c.attackTau()).epsilon (0.05));
    }
    // Monotonic
    for (int i = 0; i < 100; ++i)
    {
        CHECK (ContourGen::attackPot (l, 0.01 * (i + 1)) > ContourGen::attackPot (l, 0.01 * i));
        CHECK (ContourGen::decayPot (f, 0.01 * (i + 1)) > ContourGen::decayPot (f, 0.01 * i));
    }
    // The peak comes after about 1.3 time constants: from the 0.66 V rest
    // towards the 9.2 V target, stopped at 5.75 V + the Darlington's drop
    // (ngspice: 1.2 ms with the drawn 100 ohm and the attack pot at zero).
    const double span = ContourGen::attackSpan (ContourParams::loudness());
    CHECK (span > 1.25);
    CHECK (span < 1.40);
    for (double dial : { 0.0, 0.3, 0.7 })
    {
        ContourGen c;
        c.setParams ({});
        c.prepare (fs);
        c.setAttack (dial);
        CHECK (attackTime (dial) == Approx (span * c.attackTau()).epsilon (0.01).margin (2.0 / fs));
    }
    // Spec: about 1 ms fully counter-clockwise, 10 s fully clockwise
    CHECK (attackTime (0.0) < 0.002);
    CHECK (attackTime (1.0) > 9.0);
}

TEST_CASE ("Contour levels match the circuit (ngspice, Fig. 9-7)", "[contour]")
{
    // ngspice transient of board 2 circuit 8 (25 C), DECAY pots at 1K so that
    // the sustain settles within 200 ms; output voltages.
    struct Level { double sustain, loudness, filter; };
    const Level held[] = { { 0.0, -0.457, 0.057 }, { 0.2, 0.453, 0.734 }, { 0.4, 1.293, 1.354 },
                           { 0.6, 2.166, 2.008 }, { 0.8, 3.175, 2.790 }, { 1.0, 4.462, 3.841 } };
    for (int section = 0; section < 2; ++section)
    {
        const auto params = section == 0 ? ContourParams::loudness() : ContourParams::filter();
        INFO ((section == 0 ? "loudness" : "filter"));
        {
            ContourGen c;
            c.setParams (params);
            c.prepare (fs);
            c.reset();
            CHECK (c.output() == Approx (section == 0 ? -0.487 : 0.151).margin (0.06)); // at rest
            c.gateOn();
            double peak = -10.0;
            for (int n = 0; n < int (0.02 * fs); ++n) peak = std::fmax (peak, c.process());
            CHECK (peak == Approx (section == 0 ? 5.77 : 4.95).margin (0.08));
        }
        for (const auto& l : held)
        {
            INFO ("sustain " << l.sustain);
            ContourGen c;
            c.setParams (params);
            c.prepare (fs);
            c.reset();
            c.setDecay (0.0);
            c.setSustain (l.sustain);
            c.gateOn();
            for (int n = 0; n < int (0.2 * fs); ++n) c.process();
            CHECK (c.output() == Approx (section == 0 ? l.loudness : l.filter).margin (0.07));
        }
    }
}

TEST_CASE ("DECAY TIME spans the specification", "[contour]")
{
    // Spec: decay from about 4 ms to 35 s (read as ~3 time constants)
    ContourGen c;
    c.setParams ({});
    c.prepare (fs);
    c.setDecay (0.0);
    CHECK (3.0 * c.decayTau() < 0.006);
    c.setDecay (1.0);
    CHECK (3.0 * c.decayTau() > 20.0); // the reference's pots: 8.5 s time constant fully clockwise
}
