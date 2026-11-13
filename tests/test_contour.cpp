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

// Contour generator: ATTACK and DECAY from the drawn 1 Mohm audio pots and
// 10 uF capacitors; levels from the drawn networks (Fig. 9-7).

#include "circuit/blocks/ContourGen.h"
#include "circuit/blocks/TriggerSection.h"

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

TEST_CASE ("ATTACK and DECAY: 1 Mohm audio pots on 10 uF, 10 ms to 10 s", "[contour]")
{
    // Fig. 9-7, Table 7-3: R12 / R13 / R15 / R16 "Audio, 1 Megohm" as rheostats
    // on C5 / C2 10 uF, R7 / R42 100 in series. Specifications: "RANGE OF
    // ATTACK TIME: 10 milliseconds to 10 seconds", decay the same.
    const auto l = ContourParams::loudness(), f = ContourParams::filter();
    for (double dial : { 0.0, 0.2, 0.5, 0.8, 1.0 })
    {
        INFO ("dial " << dial);
        CHECK (ContourGen::attackPot (l, dial) == Approx (1.0e6 * taper::audio (dial)).margin (1e-6));
        CHECK (ContourGen::decayPot (f, dial) == Approx (1.0e6 * taper::audio (dial)).margin (1e-6));
    }
    CHECK (ContourGen::attackPot (l, 0.5) == Approx (100.0e3)); // 10 % of the track at half rotation: 1 s
    for (int i = 0; i < 100; ++i)
    {
        CHECK (ContourGen::attackPot (l, 0.01 * (i + 1)) > ContourGen::attackPot (l, 0.01 * i));
        CHECK (ContourGen::decayPot (f, 0.01 * (i + 1)) > ContourGen::decayPot (f, 0.01 * i));
    }
    // The peak comes after about 1.1 time constants: from the 0.66 V rest
    // towards the 9.2 V target, stopped at the peak detector's 5.2 V plus the
    // Darlington's drop.
    const double span = ContourGen::attackSpan (ContourParams::loudness());
    CHECK (span > 1.0);
    CHECK (span < 1.2);
    for (double dial : { 0.0, 0.3, 0.7 })
    {
        ContourGen c;
        c.setParams ({});
        c.prepare (fs);
        c.setAttack (dial);
        CHECK (attackTime (dial) == Approx (span * c.attackTau()).epsilon (0.01).margin (2.0 / fs));
        // the 10-90 % rise is a little under a time constant
        CHECK (attackRise (l, dial) == Approx (0.85 * c.attackTau()).epsilon (0.05).margin (3.0 / fs));
    }
    // The specification's range: under 10 ms fully counter-clockwise (the
    // drawn 100 ohm gives 1.4 ms), 10 s fully clockwise.
    CHECK (attackTime (0.0) < 0.010);
    CHECK (attackTime (1.0) == Approx (11.0).margin (1.5));
}

TEST_CASE ("Contour levels follow the drawn networks (Fig. 9-7, service manual 2.10)", "[contour]")
{
    // Output voltages held at six SUSTAIN settings: the SUSTAIN dividers (R19 /
    // R18 5K, R57 / R13 4.7K, 4.7K or R1 3K with R17 150K to -10 V), the sustain
    // follower and the output follower; ngspice on the drawn circuit
    // (tools/spice/contour.cir).
    struct Level { double sustain, loudness, filter; };
    const Level held[] = { { 0.0, -0.368, 0.132 }, { 0.2, 0.539, 0.807 }, { 0.4, 1.376, 1.426 },
                           { 0.6, 2.247, 2.078 }, { 0.8, 3.253, 2.858 }, { 1.0, 4.539, 3.907 } };
    for (int section = 0; section < 2; ++section)
    {
        const auto params = section == 0 ? ContourParams::loudness() : ContourParams::filter();
        INFO ((section == 0 ? "loudness" : "filter"));
        {
            ContourGen c;
            c.setParams (params);
            c.prepare (fs);
            c.reset();
            CHECK (c.output() == Approx (section == 0 ? -0.466 : 0.157).margin (0.03)); // at rest (ngspice)
            c.gateOn();
            double peak = -10.0;
            for (int n = 0; n < int (0.02 * fs); ++n) peak = std::fmax (peak, c.process());
            // "When the voltage on R33 reaches approximately 5 volts, CR3 conducts"
            // (2.10): R24 6.8K / R33 5.6K against R27 / R29 16.9K to -10 V, the
            // germanium CR6 / CR3 and the flip-flop's Vbe.
            CHECK (peak == Approx (section == 0 ? 5.223 : 4.434).margin (0.04)); // ngspice
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
            CHECK (c.output() == Approx (section == 0 ? l.loudness : l.filter).margin (0.04));
        }
    }
}

TEST_CASE ("DECAY TIME spans the specification", "[contour]")
{
    // Specifications: decay from 10 milliseconds to 10 seconds. The time
    // constant: 1.3 ms with the pot at zero, 10 s with its whole megohm.
    ContourGen c;
    c.setParams ({});
    c.prepare (fs);
    c.setDecay (0.0);
    CHECK (c.decayTau() < 0.010);
    c.setDecay (1.0);
    CHECK (c.decayTau() == Approx (10.0).epsilon (0.01));
}

TEST_CASE ("Trigger section: C7's delays on both edges (ngspice)", "[contour]")
{
    // Fig. 9-7: the bus through CR10 and R60 100K into C7 .1 uF, R55 47K into
    // Q20's base. ngspice on the drawn circuit (tools/spice/trigger.cir), C7
    // starting at -10 V: the V-trigger rises 7.9 ms after the key and falls
    // 10.8 ms after its release.
    constexpr double rate = 96000.0;
    TriggerSection t;
    t.prepare (rate);
    t.reset();
    auto timeTo = [&] (bool key, bool state) {
        for (int i = 0; i < int (rate); ++i)
            if (t.process (key) == state) return 1000.0 * i / rate;
        return 1.0e9;
    };
    const double on = timeTo (true, true);
    for (int i = 0; i < int (0.05 * rate); ++i) t.process (true);
    const double off = timeTo (false, false);
    INFO ("on after " << on << " ms, off after " << off << " ms");
    CHECK (on == Approx (7.92).margin (0.3));
    CHECK (off == Approx (10.79).margin (0.5));
    // Struck again 20 ms after the release: C7 has not drifted far yet.
    for (int i = 0; i < int (0.02 * rate); ++i) t.process (false);
    const double again = timeTo (true, true);
    INFO ("struck again: " << again << " ms");
    CHECK (again < 1.5);
}
