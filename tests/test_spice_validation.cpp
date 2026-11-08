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

// Validates the MNA solver and device models against ngspice on small
// reference circuits (identical netlists, identical device parameters).

#include "SpiceRunner.h"
#include "circuit/solver/MnaCircuit.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace mm;

namespace
{
constexpr double fsSim = 192000.0;
constexpr double hSim = 1.0 / fsSim;

const char* npnModel = ".model QN NPN(IS=6.734f BF=220 BR=0.7371 VAF=74.03)\n";
const char* diodeModel = ".model D1 D(IS=2.52n N=1.752)\n";

BjtParams npnParams() { return { 6.734e-15, 220.0, 0.7371, 74.03, false }; }

template <typename Circuit, typename Drive, typename Probe>
std::vector<double> simulate (Circuit& c, double stop, Drive&& drive, Probe&& probe, std::vector<double>& times)
{
    c.setTimeStep (hSim);
    drive (0.0);
    REQUIRE (c.solveDC());
    std::vector<double> out;
    const int n = int (std::round (stop / hSim));
    for (int i = 1; i <= n; ++i)
    {
        const double t = i * hSim;
        drive (t);
        c.step();
        times.push_back (t);
        out.push_back (probe());
    }
    return out;
}
} // namespace

TEST_CASE ("RC low-pass matches ngspice", "[spice]")
{
    if (! mmtest::ngspiceAvailable()) SKIP ("ngspice not installed");

    const auto ref = mmtest::runNgspiceTran ("rc",
        "V1 in 0 SIN(0 1 500)\nR1 in out 1k\nC1 out 0 1u\n", { "v(out)" }, hSim, 5e-3);
    REQUIRE (ref);

    MnaCircuit<2, 1> c;
    const int vin = c.addVoltageSource (1, 0, 0.0);
    c.addResistor (1, 2, 1000.0);
    c.addCapacitor (2, 0, 1e-6);

    std::vector<double> t;
    auto sim = simulate (c, 5e-3, [&] (double time) { c.setVoltageSource (vin, std::sin (2 * M_PI * 500 * time)); },
                         [&] { return c.v (2); }, t);
    std::vector<double> r;
    for (double time : t) r.push_back (ref->at (0, time));
    CHECK (mmtest::relativeRmsError (sim, r) < 2e-3);
}

TEST_CASE ("Diode clipper matches ngspice", "[spice]")
{
    if (! mmtest::ngspiceAvailable()) SKIP ("ngspice not installed");

    const std::string net = std::string ("V1 in 0 SIN(0 2 1000)\nR1 in out 2.2k\nC1 out 0 10n\n"
                                         "D1 out 0 D1\nD2 0 out D1\n") + diodeModel;
    const auto ref = mmtest::runNgspiceTran ("clipper", net, { "v(out)" }, hSim, 3e-3);
    REQUIRE (ref);

    MnaCircuit<2, 1> c;
    const int vin = c.addVoltageSource (1, 0, 0.0);
    c.addResistor (1, 2, 2200.0);
    c.addCapacitor (2, 0, 10e-9);
    c.addDiode (2, 0, { 2.52e-9, 1.752 });
    c.addDiode (0, 2, { 2.52e-9, 1.752 });

    std::vector<double> t;
    auto sim = simulate (c, 3e-3, [&] (double time) { c.setVoltageSource (vin, 2.0 * std::sin (2 * M_PI * 1000 * time)); },
                         [&] { return c.v (2); }, t);
    std::vector<double> r;
    for (double time : t) r.push_back (ref->at (0, time));
    CHECK (mmtest::relativeRmsError (sim, r) < 5e-3);
}

TEST_CASE ("BJT differential pair with collector capacitor matches ngspice", "[spice]")
{
    if (! mmtest::ngspiceAvailable()) SKIP ("ngspice not installed");

    // Long-tailed pair, 1 mA tail, 4.7k loads to +15 V, 100 nF across the collectors
    // (a single ladder-like stage). Drive large enough to reach the tanh region.
    const std::string net = std::string (
        "VCC vcc 0 15\nVB1 b1 0 SIN(0 0.08 300)\n"
        "Q1 c1 b1 e QN\nQ2 c2 0 e QN\nIT e vee 1m\nVEE vee 0 -15\n"
        "RC1 vcc c1 4.7k\nRC2 vcc c2 4.7k\nCC c1 c2 100n\n") + npnModel;
    const auto ref = mmtest::runNgspiceTran ("diffpair", net, { "v(c1)-v(c2)" }, hSim, 10e-3);
    REQUIRE (ref);

    // nodes: 1 vcc, 2 b1, 3 c1, 4 c2, 5 e, 6 vee
    MnaCircuit<6, 3> c;
    c.addVoltageSource (1, 0, 15.0);
    const int vb = c.addVoltageSource (2, 0, 0.0);
    c.addVoltageSource (6, 0, -15.0);
    c.addBjt (3, 2, 5, npnParams());
    c.addBjt (4, 0, 5, npnParams());
    c.addCurrentSource (5, 6, 1e-3);
    c.addResistor (1, 3, 4700.0);
    c.addResistor (1, 4, 4700.0);
    c.addCapacitor (3, 4, 100e-9);

    std::vector<double> t;
    auto sim = simulate (c, 10e-3, [&] (double time) { c.setVoltageSource (vb, 0.08 * std::sin (2 * M_PI * 300 * time)); },
                         [&] { return c.v (3) - c.v (4); }, t);
    std::vector<double> r;
    for (double time : t) r.push_back (ref->at (0, time));
    CHECK (mmtest::relativeRmsError (sim, r) < 5e-3);
}
