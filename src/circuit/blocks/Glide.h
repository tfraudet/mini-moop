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
// Keyboard track and hold, and glide (board 2 circuit 1, Fig. 9-7; service
// manual 2.19). The differential amplifier Q23 / Q14 / Q24 compares the key's
// voltage with the output and charges the storage capacitor C6 1 uF through
// R61 330, the GLIDE pot (R2 5M, between pins 8 and 7, shorted by the GLIDE
// switch) and the sampling FET Q13; the FET follower Q10 is the output.
//
// A key change saturates the amplifier: Q24 on pulls its collector to the
// +10 V rail, Q24 off leaves it on R59 3.9K to +10 V and R54 1.5K to -10 V,
// at -4.44 V. C6 therefore charges through the pot towards one of these two
// voltages until the loop comes back into its linear range: an exponential
// that is cut off long before its end, nearly a straight slide, faster up than
// down and depending on where on the keyboard it happens. "RATE OF KEYBOARD
// GLIDE: continuously variable from 1 millisecond to 1 second/octave"
// (specifications).
//
// The output is not C6 itself but the FET follower Q10 on R18 3.9K: ngspice
// on the drawn circuit gives out = 0.927 C6 + 0.88 V (tools/spice/
// keyboard.cir). C6 sits below the output, closer to the -4.44 V rail than to
// the +9.87 V one, which makes the glide down nearly twice as slow as the
// glide up (the bench: 0.58 and 1.07 s an octave at 5M, from 1 to 2 V).
//
// "When the key is released, Q13 is turned off and the pitch voltage is held"
// (2.19): the glide stops where it is when the last key is released, and the
// held voltage only drifts by the two FETs' gate leakages, Q13's out of C6
// and Q10's into it: either way, by a few cents a minute ("greater than 1
// semitone per minute" is a fault, Folkman).
//
// Values are keyboard volts: 0 V on the lowest key (the string's grounded
// end), 1 V an octave.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

struct GlideParams
{
    double c        = 1.0e-6;   // storage capacitor C6, 1 uF polyester
    double rSeries  = 580.0;    // R61 330 and Q13's channel (2N4303 on, about 250 ohm: data sheet)
    double rPot     = 5.0e6;    // GLIDE pot R2: 5 Mohm, "No. 1 taper" (Table 7-3)
    double vUp      = 9.87;     // Q24 saturated on the +10 V rail (ngspice)
    double vDown    = -4.44;    // Q24 off: R59 3.9K to +10 V against R54 1.5K to -10 V
    double followerGain   = 0.927; // Q10 on R18 3.9K: out = gain C6 + offset (ngspice, keyboard.cir)
    double followerOffset = 0.88;
    double loopGain = 2000.0;   // the amplifier's gain in its linear range (estimated: it only shapes the last millivolts)
    double leakage  = 30.0e-12; // the net of Q13's and Q10's gate leakages, A (2N4303 data sheet: tens of pA at 25 C);
                                // positive out of C6

    static GlideParams drawn (Rng& rng, const GlideParams& nominal) noexcept
    {
        GlideParams p = nominal;
        p.c = rng.tolerance (nominal.c, 0.10);
        p.rPot = rng.tolerance (nominal.rPot, 0.20);
        const double r59 = rng.tolerance (3.9e3, 0.05), r54 = rng.tolerance (1.5e3, 0.05);
        p.vDown = (10.0 * r54 - 10.0 * r59) / (r59 + r54);
        // FETs differ widely, and which of the two leaks more decides the sign.
        p.leakage = nominal.leakage * std::exp (0.7 * rng.normal()) * (rng.uniform() < 0.5 ? -1.0 : 1.0);
        return p;
    }

    /** GLIDE pot resistance (ohm) at a knob position 0..1. */
    double potAt (double pos) const noexcept { return rPot * taper::audio (pos); }
};

class Glide
{
public:
    void prepare (double sampleRate) noexcept { h = 1.0 / sampleRate; update(); }
    void setParams (const GlideParams& params) noexcept { p = params; update(); }
    void setTime (double pos) noexcept { pot = pos; update(); }
    void setEnabled (bool on) noexcept { enabled = on; update(); }
    void reset (double v) noexcept { y = v; }

    /** One sample. `target`: the key's voltage; `tracking`: a key is down (Q13
        conducts). Returns the held voltage. */
    double process (double target, bool tracking) noexcept
    {
        if (! tracking)
        {
            y -= droop; // hold: only the gate leakage moves C6
            return y;
        }
        const double e = target - y;
        if (e == 0.0)
            return y;
        // Saturated: C6 through the pot from the amplifier's rail, seen at the
        // output through the follower. Linear range: the same resistance from
        // loopGain times the error.
        const double rail = p.followerGain * (e > 0.0 ? p.vUp : p.vDown) + p.followerOffset;
        const double sat = (rail - y) * kSat;
        const double lin = e * kLin;
        y += std::abs (lin) < std::abs (sat) ? lin : sat;
        return y;
    }

    double value() const noexcept { return y; }

    /** Resistance between the amplifier and C6 for the current settings (ohm). */
    double resistance() const noexcept { return p.rSeries + (enabled ? p.potAt (pot) : 0.0); }

private:
    void update() noexcept
    {
        const double rc = resistance() * p.c;
        kSat = 1.0 - std::exp (-h / rc);
        kLin = 1.0 - std::exp (-h * p.loopGain / rc);
        droop = p.followerGain * p.leakage / p.c * h;
    }

    GlideParams p;
    double h = 1.0 / 48000.0, pot = 0.0, kSat = 1.0, kLin = 1.0, droop = 0.0, y = 0.0;
    bool enabled = false;
};

} // namespace mm
