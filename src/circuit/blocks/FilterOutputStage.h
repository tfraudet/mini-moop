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

#pragma once
// Board 4's output buffer, the "gain recovery amplifier" after the ladder
// (Fig. 9-11): C5/C1 0.22 uF couple the top capacitor C3 into the followers
// Q8/Q6 (bases biased by R31/R38 47K, emitters on R24 220K / R20 100K), which
// drive the pair Q7/Q5 (tail R26 1K to -10 V, about 9 mA). Q7's and Q8's
// collectors are the output, loaded by R3 1K and R7 180 into C9 (AC ground)
// and R6 47 to +10 V. There is no op-amp in this stage.
//
// Its transfer, from the top capacitor's AC voltage to the output's AC
// voltage, is tabulated from ngspice (TIS97 model, 25 C), with the output
// loaded by the VCA's input (R2 82K + R34 820) and the emphasis path at
// EMPHASIS 0 (R14 50K), and the bases at the offsets their base currents put
// on R31/R38 (-8.5 and -13.6 mV). Small-signal gain -68.3 (the full board
// in ngspice: -68.26); it clips at +6.4 V (Q7 cut off) and -3.7 V (Q7 and Q8
// saturated): asymmetric, as on the instrument. Generated from
// spikes/ngspice-filter-output/outstage.cir; tests/test_filter_output.cpp
// regenerates it with ngspice and compares. Do not edit by hand.

#include <array>
#include <cmath>

namespace mm
{

struct FilterOutputStage
{
    static constexpr double vdMin = -0.300;  // top capacitor's AC voltage at the first point (V)
    static constexpr double vdStep = 0.002;  // V between points
    static constexpr int points = 301;
    static constexpr double smallSignalGain = -68.2538; // d(out)/d(vd) at 0 (ngspice)

    /** Output's AC voltage (V) for the top capacitor's AC voltage `vd` (V). */
    static constexpr std::array<double, points> table {
        6.429815, 6.429779, 6.429738, 6.429696, 6.429651, 6.429602,
        6.429550, 6.429494, 6.429434, 6.429370, 6.429301, 6.429227,
        6.429147, 6.429062, 6.428970, 6.428871, 6.428764, 6.428649,
        6.428525, 6.428392, 6.428249, 6.428095, 6.427928, 6.427749,
        6.427555, 6.427347, 6.427122, 6.426879, 6.426617, 6.426335,
        6.426031, 6.425702, 6.425348, 6.424965, 6.424553, 6.424107,
        6.423626, 6.423108, 6.422547, 6.421943, 6.421290, 6.420586,
        6.419825, 6.419004, 6.418118, 6.417161, 6.416129, 6.415014,
        6.413810, 6.412511, 6.411109, 6.409595, 6.407961, 6.406198,
        6.404295, 6.402241, 6.400024, 6.397632, 6.395051, 6.392267,
        6.389263, 6.386022, 6.382527, 6.378758, 6.374693, 6.370311,
        6.365587, 6.360495, 6.355008, 6.349096, 6.342728, 6.335869,
        6.328484, 6.320535, 6.311980, 6.302777, 6.292879, 6.282237,
        6.270800, 6.258513, 6.245318, 6.231155, 6.215959, 6.199663,
        6.182197, 6.163487, 6.143457, 6.122028, 6.099115, 6.074636,
        6.048500, 6.020619, 5.990900, 5.959249, 5.925572, 5.889771,
        5.851752, 5.811417, 5.768672, 5.723422, 5.675576, 5.625045,
        5.571743, 5.515588, 5.456504, 5.394420, 5.329271, 5.261001,
        5.189559, 5.114903, 5.037001, 4.955829, 4.871373, 4.783629,
        4.692604, 4.598314, 4.500785, 4.400056, 4.296175, 4.189198,
        4.079195, 3.966242, 3.850428, 3.731847, 3.610605, 3.486813,
        3.360593, 3.232071, 3.101382, 2.968666, 2.834067, 2.697737,
        2.559831, 2.420509, 2.279933, 2.138269, 1.995686, 1.852355,
        1.708448, 1.564139, 1.419604, 1.275016, 1.130552, 0.986386,
        0.842692, 0.699644, 0.557413, 0.416167, 0.276075, 0.137299,
        0.000000, -0.135665, -0.269545, -0.401490, -0.531361, -0.659019,
        -0.784336, -0.907187, -1.027458, -1.145040, -1.259833, -1.371746,
        -1.480697, -1.586614, -1.689433, -1.789101, -1.885576, -1.978826,
        -2.068827, -2.155568, -2.239048, -2.319275, -2.396267, -2.470052,
        -2.540666, -2.608154, -2.672569, -2.733971, -2.792428, -2.848011,
        -2.900800, -2.950877, -2.998328, -3.043244, -3.085716, -3.125839,
        -3.163707, -3.199416, -3.233062, -3.264739, -3.294541, -3.322561,
        -3.348890, -3.373615, -3.396822, -3.418596, -3.439015, -3.458156,
        -3.476093, -3.492894, -3.508623, -3.523338, -3.537092, -3.549933,
        -3.561902, -3.573033, -3.583357, -3.592900, -3.601688, -3.609747,
        -3.617107, -3.623801, -3.629868, -3.635351, -3.640297, -3.644753,
        -3.648766, -3.652381, -3.655639, -3.658578, -3.661234, -3.663636,
        -3.665811, -3.667783, -3.669573, -3.671199, -3.672677, -3.674020,
        -3.675241, -3.676351, -3.677358, -3.678272, -3.679099, -3.679847,
        -3.680521, -3.681126, -3.681668, -3.682150, -3.682577, -3.682951,
        -3.683277, -3.683557, -3.683794, -3.683990, -3.684148, -3.684269,
        -3.684357, -3.684412, -3.684437, -3.684432, -3.684400, -3.684342,
        -3.684259, -3.684153, -3.684024, -3.683873, -3.683703, -3.683513,
        -3.683305, -3.683079, -3.682836, -3.682577, -3.682303, -3.682015,
        -3.681712, -3.681397, -3.681068, -3.680727, -3.680375, -3.680011,
        -3.679636, -3.679252, -3.678857, -3.678453, -3.678039, -3.677617,
        -3.677186, -3.676747, -3.676301, -3.675846, -3.675385, -3.674916,
        -3.674440, -3.673958, -3.673470, -3.672975, -3.672475, -3.671968,
        -3.671456, -3.670939, -3.670416, -3.669888, -3.669355, -3.668818,
        -3.668275, -3.667729, -3.667177, -3.666622, -3.666062, -3.665498,
        -3.664929,
    };

    /** The transfer and its derivative, by cubic Hermite interpolation
        (Catmull-Rom slopes): continuous slope for the emphasis loop's Newton
        solve. Flat beyond the table. */
    static double transfer (double vd, double& slope) noexcept
    {
        const double x = (vd - vdMin) / vdStep;
        if (x <= 0.0) { slope = 0.0; return table.front(); }
        if (x >= double (points - 1)) { slope = 0.0; return table.back(); }
        const int i = int (x);
        const double t = x - double (i);
        auto at = [] (int j) { return table[size_t (j < 0 ? 0 : j >= points ? points - 1 : j)]; };
        const double y0 = at (i), y1 = at (i + 1);
        const double m0 = 0.5 * (at (i + 1) - at (i - 1)), m1 = 0.5 * (at (i + 2) - at (i));
        const double t2 = t * t, t3 = t2 * t;
        const double h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
        slope = ((6 * t2 - 6 * t) * y0 + (3 * t2 - 4 * t + 1) * m0 + (-6 * t2 + 6 * t) * y1 + (3 * t2 - 2 * t) * m1) / vdStep;
        return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
    }
};

} // namespace mm
