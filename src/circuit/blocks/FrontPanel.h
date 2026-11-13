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
// The front panel's resistor networks (Fig. 9-16, Table 7-2) against the
// boards' inputs (Fig. 9-3, 9-11): what each knob and switch adds to an
// oscillator's or the filter's control voltage. Octaves throughout: the
// oscillators' summers take 1 V an octave through the keyboard's 51.1K (R27),
// the filter's control node through 100K.

#include "../components/Passives.h"

#include <array>

namespace mm
{

struct FrontPanel
{
    // --- Oscillators: the summing amplifiers' input resistors (Fig. 9-3) ----
    static constexpr double rKeyboard = 51.1e3; // R27 / R80 / R129, 1 % matched: 1 V an octave
    static constexpr double rTuneIn   = 560.0e3; // R21 / R88 / R120
    static constexpr double rFreqIn   = 220.0e3; // R95 (oscillator 2's FREQUENCY)
    static constexpr double rOsc3Tune = 51.0e3;  // R181: oscillator 3's FREQUENCY into IC8
    static constexpr double rOsc3Wide = 15.0e3;  // R180: in parallel with R181 when OSC. 3 CONTROL is off

    // --- RANGE (Fig. 9-16; service manual 2.15) ------------------------------
    // One string for the three switches: R29 4.75K 1 % then R25..R28 1K
    // (a matched set), fed from the -5 V reference and trimmed as a whole by
    // the OCTAVE trimmer R168. The tap at the -5 V end is LO, then 32', 16',
    // 8', 4', 2': LO is 4.75 octaves below 32'.
    static constexpr double rOctave = 1.0e3;
    static constexpr double rLo     = 4.75e3;
    /** Nominal octaves of each RANGE position above 8': LO, 32', 16', 8', 4', 2'. */
    static constexpr std::array<double, 6> rangeOctaves { -2.0 - rLo / rOctave, -2.0, -1.0, 0.0, 1.0, 2.0 };

    // --- TUNE: R1 5K linear from +10 V to R22 5.1K to ground ------------------
    static constexpr double rTunePot = 5.0e3, rTuneFoot = 5.1e3;
    /** Octaves from the centre for a knob position 0..1 (+/-2.7 semitones). */
    static double tuneOctaves (double pos, double rPot = rTunePot) noexcept
    {
        return 10.0 * rPot * (pos - 0.5) / (rPot + rTuneFoot) * rKeyboard / rTuneIn;
    }

    // --- FREQUENCY: R4 / R5 5K linear from +10 V to R35 / R41 3.3K to ground --
    static constexpr double rFreqPot = 5.0e3, rFreqFoot = 3.3e3;
    /** Octaves from the centre for a knob position 0..1 (+/-8.4 semitones). */
    static double frequencyOctaves (double pos, double rPot = rFreqPot) noexcept
    {
        return 10.0 * rPot * (pos - 0.5) / (rPot + rFreqFoot) * rKeyboard / rFreqIn;
    }
    /** How much wider oscillator 3's FREQUENCY is with OSC. 3 CONTROL off: R180
        joins R181 at IC8's input (4.4: "a frequency sweep of 6 octaves rather
        than one octave", owner's manual). */
    static constexpr double osc3WideRatio = 1.0 + rOsc3Tune / rOsc3Wide;

    // --- Filter control node (Fig. 9-16, 9-11) --------------------------------
    static constexpr double rUnit    = 100.0e3; // 1 V an octave: R53 || R54
    static constexpr double rKbd1    = 300.0e3; // R53, KEYBOARD CONTROL 1
    static constexpr double rKbd2    = 150.0e3; // R54, KEYBOARD CONTROL 2
    static constexpr double rCutoff  = 200.0e3; // R55, from CUTOFF R11 (5K linear, +10 V to -10 V)
    static constexpr double rContour = 47.0e3;  // R74 (board 4), from AMOUNT OF CONTOUR R17's wiper
    static constexpr double rFilterMod = 33.0e3; // R52, from the modulation line
    static constexpr double rExtFilter = 100.0e3; // R51, from the external filter control jack
};

} // namespace mm
