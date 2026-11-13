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
// The filter's cutoff converter (board 4, circuit 6, Fig. 9-11; service manual
// 2.8): "a matched NPN-PNP pair Q26 and Q28". The control voltages are summed
// by their resistors on the control node, which R49 (SCALE) and R70 1.8K return
// to ground: a few millivolts per octave on the base of the PNP follower Q26
// (TIS93). Its emitter, fed through R46 68K from the RANGE trim R39, drives the
// base of Q28 (TIS92), whose collector current is the ladder's standing
// current:
//
//   I = (Is_n / Is_p) Ie exp (Vb / Vt)
//
// The two junctions' saturation currents drift together, so the current at
// Vb = 0 (the CUTOFF knob at its centre, no other control) hardly moves with
// temperature. But there is no temperature-compensating resistor here, unlike
// the oscillators' R20: the scale, 1 / Vt, falls by 0.33 % a kelvin, so the
// filter's octaves stretch and shrink around that point as the board warms.
//
// The current tops out: Q28's collector feeds the bottom pair's emitters (about
// 1.3 V) through R60 680, so it saturates near 1.75 mA, a cutoff of some
// 38 kHz ("Pitch should rise from 440Hz to 35kHz +/- 10kHz" with AMOUNT OF
// CONTOUR fully clockwise, 5.17).
//
// `cv` is in octaves as calibrated at 25 C: 1 for a volt through 100K, which is
// what KEYBOARD CONTROL 1 and 2 together (R53 300K, R54 150K) present.

#include <cmath>

namespace mm
{

class CutoffConverter
{
public:
    /** RANGE (R39): the current at cv = 0. SCALE (R49): octaves of current for
        an octave of cv at 25 C. */
    void setTrims (double currentAtZero, double scaleTrim) noexcept { i0 = currentAtZero; scale = scaleTrim; }
    double currentAtZero() const noexcept { return i0; }
    double scaleTrim() const noexcept { return scale; }

    void setTemperature (double kelvin) noexcept { tempRatio = 298.15 / kelvin; }

    /** Ladder standing current (A) for a control voltage in octaves. */
    double current (double cv) const noexcept
    {
        const double i = i0 * std::exp2 (std::fmin (cv * scale * tempRatio, 40.0));
        const double r = i / iMax, r2 = r * r;
        return i / std::sqrt (std::sqrt (1.0 + r2 * r2)); // Q28 saturating against R60
    }

    /** Where Q28 saturates: the bottom pair's emitters (1.3 V) less its
        saturation voltage, over R60 680. */
    static constexpr double iMax = (1.3 - 0.1) / 680.0;

private:
    double i0 = 4.0e-5, scale = 1.0, tempRatio = 1.0;
};

} // namespace mm
