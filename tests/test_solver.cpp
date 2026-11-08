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

#include "circuit/components/Passives.h"
#include "circuit/solver/DenseLU.h"
#include "circuit/solver/MnaCircuit.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace mm;
using Catch::Approx;

TEST_CASE ("LU solves a pivoting-required system", "[solver]")
{
    Matrix<3> A;
    A (0, 0) = 0; A (0, 1) = 2; A (0, 2) = 1;
    A (1, 0) = 1; A (1, 1) = 1; A (1, 2) = 1;
    A (2, 0) = 3; A (2, 1) = 0; A (2, 2) = -1;
    // x = (1, 2, 3)  ->  b = (2*2 + 3, 1 + 2 + 3, 3 - 3)
    Vector<3> b { 7, 6, 0 };
    REQUIRE (luSolveInPlace (A, b));
    CHECK (b[0] == Approx (1.0));
    CHECK (b[1] == Approx (2.0));
    CHECK (b[2] == Approx (3.0));
}

TEST_CASE ("RC step response matches the analytic solution", "[solver]")
{
    const double R = 10e3, C = 100e-9, fs = 96000.0;
    MnaCircuit<2, 1> c;
    const int src = c.addVoltageSource (1, 0, 0.0);
    c.addResistor (1, 2, R);
    c.addCapacitor (2, 0, C);
    c.setTimeStep (1.0 / fs);
    REQUIRE (c.solveDC());
    c.setVoltageSource (src, 1.0);
    double maxErr = 0.0;
    for (int i = 1; i < 2000; ++i)
    {
        c.step();
        const double t = i / fs;
        maxErr = std::fmax (maxErr, std::abs (c.v (2) - (1.0 - std::exp (-t / (R * C)))));
    }
    // Trapezoidal rule with an ideal step input has an initial half-step error;
    // after that it tracks the analytic curve closely.
    CHECK (maxErr < 0.06);
}

TEST_CASE ("Tolerance draws are deterministic and bounded", "[components]")
{
    Rng a (42), b (42);
    for (int i = 0; i < 1000; ++i)
    {
        const double va = a.tolerance (1000.0, 0.05);
        CHECK (va == b.tolerance (1000.0, 0.05));
        CHECK (va >= 950.0);
        CHECK (va <= 1050.0);
    }
}

TEST_CASE ("Audio taper hits 10 % at mid rotation", "[components]")
{
    CHECK (taper::audio (0.0) == Approx (0.0));
    CHECK (taper::audio (0.5) == Approx (0.1));
    CHECK (taper::audio (1.0) == Approx (1.0));
}
