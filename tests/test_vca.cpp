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
#include <utility>

using namespace mm;
using Catch::Approx;

// Reference values: ngspice at 25 C on the netlist transcribed from Fig. 9-11
// (tools/spice/board4-vca.lib; vca_op.cir, vca_large.cir), the output loaded
// by C2, R75 1K and the VOLUME pot's 5K.

TEST_CASE ("VCA: Q18's tail current against the loudness contour (ngspice)", "[vca]")
{
    struct Point { double volts, amps; };
    const Point ngspice[] = { { 1.0, 96.46e-6 }, { 2.0, 239.98e-6 }, { 3.0, 385.07e-6 }, { 4.0, 530.61e-6 },
                              { 4.5, 552.26e-6 }, { 5.0, 549.20e-6 }, { 5.5, 544.62e-6 } };
    Vca v;
    v.setParams ({});
    for (const auto& p : ngspice)
    {
        INFO (p.volts << " V");
        CHECK (v.tailCurrent (p.volts) == Approx (p.amps).epsilon (0.03));
    }
    // Closed at the contour's rest (-0.5 V); saturated from about 4.3 V: the
    // attack's peak and the top of the decay play at full gain.
    CHECK (v.tailCurrent (-0.5) < 1.0e-9);
    CHECK (v.tailCurrent (0.2) < 2.0e-6);
}

TEST_CASE ("VCA: gain from the filter's output against the contour (ngspice)", "[vca]")
{
    const double fs = 192000.0;
    auto gain = [fs] (double contour) {
        Vca v;
        v.setParams ({});
        v.prepare (fs);
        double lo = 1e9, hi = -1e9;
        for (int n = 0; n < int (0.06 * fs); ++n)
        {
            const double y = v.process (0.05 * std::sin (2.0 * M_PI * 1000.0 * n / fs), contour);
            if (n > int (0.04 * fs)) { lo = std::fmin (lo, y); hi = std::fmax (hi, y); }
        }
        return 0.5 * (hi - lo) / 0.05;
    };
    struct Point { double volts, gain; };
    const Point ngspice[] = { { 0.5, 0.1027 }, { 1.0, 0.3564 }, { 2.0, 0.8788 }, { 3.0, 1.3967 },
                              { 4.0, 1.9057 }, { 4.5, 1.9802 }, { 5.0, 1.9697 }, { 6.0, 1.9363 } };
    Vca model;
    model.setParams ({});
    for (const auto& p : ngspice)
    {
        INFO (p.volts << " V: " << gain (p.volts) << " against " << p.gain);
        CHECK (20.0 * std::log10 (gain (p.volts) / p.gain) == Approx (0.0).margin (1.0)); // dB
        CHECK (model.smallSignalGain (p.volts) == Approx (gain (p.volts)).epsilon (0.03));
    }
    CHECK (gain (-0.5) < 1.0e-3); // closed between notes ("dynamic range 80 dB")
}

TEST_CASE ("VCA: the output stage clips softly near 3 V each way (ngspice)", "[vca]")
{
    // 1 kHz sine from the filter at a 5 V contour; Q17's collector's swing
    // about its rest: +1.85 / -1.80 V for 1 V, +2.87 / -2.71 for 2 V,
    // +3.05 / -2.84 for 8 V (the pair fully switched).
    const double fs = 192000.0;
    auto peaks = [fs] (double amplitude) {
        Vca v;
        v.setParams ({});
        v.prepare (fs);
        double lo = 1e9, hi = -1e9;
        for (int n = 0; n < int (0.06 * fs); ++n)
        {
            const double y = v.process (amplitude * std::sin (2.0 * M_PI * 1000.0 * n / fs), 5.0);
            if (n > int (0.04 * fs)) { lo = std::fmin (lo, y); hi = std::fmax (hi, y); }
        }
        return std::pair<double, double> { hi, lo };
    };
    struct Point { double amplitude, hi, lo; };
    for (const auto& p : { Point { 0.5, 0.97, -0.96 }, Point { 1.0, 1.85, -1.80 }, Point { 2.0, 2.87, -2.71 }, Point { 8.0, 3.05, -2.84 } })
    {
        const auto [hi, lo] = peaks (p.amplitude);
        INFO (p.amplitude << " V in: " << hi << " / " << lo);
        CHECK (hi == Approx (p.hi).margin (0.25));
        CHECK (lo == Approx (p.lo).margin (0.25));
    }
    Vca v;
    v.setParams ({});
    CHECK (v.swing() == Approx (2.83).margin (0.05));
}
