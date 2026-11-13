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
// External input pre-amplifier and OVERLOAD lamp driver: board 4, circuit 12
// (service manual Fig. 9-11).
//
// Pre-amplifier. The EXTERNAL INPUT VOLUME pot R9 (1 Mohm audio, front panel)
// feeds pin 11: R78 1k, C23 0.1 uF, R66 100k to ground (the load seen by the
// pot wiper, and a 16 Hz high-pass). A TIS97 differential pair (Q27/Q32,
// 63 uA tail through R58 150k) drives the 2N4058 output transistor Q33, pulled
// down by R57 10k. Feedback R61 200k / R62 1k gives a gain of 1 + 200k/1k =
// 201; C26 220 uF in series with R62 brings the DC gain back to 1 below
// ~0.7 Hz. The output saturates near the +10 V rail (Q33) and, on the
// negative side, at R57's pull-down against the mixer load (about -7.7 V).
// C20 1 uF couples it to the mixer switch S10 and its 33k summing resistor.
//
// Lamp driver. Q25 (emitter follower from the pre-amp output through R53 10k)
// charges C14 0.47 uF to the positive peaks. R56 100k and R48 680k to -10 V
// bias the PNP Q31, whose emitter holds the lamp switch Q34's base against
// R69 10k from +15 V; Q34 sinks the 16ESB lamp through R72 47 ohm. Q31's base
// current (R69's 1.5 mA over its gain) comes back up the divider and lifts
// its base by some 0.7 V: the lamp comes on at a pre-amp output peak of 1.5 V
// (the service manual, 2.12: "approximately 1.2 volts"), not the 2.1 V the
// divider alone gives, and C14 relaxes towards -4.5 V, not -10 V. ngspice on
// the drawn circuit (tools/spice/overload.cir): the lamp's current against
// C14's voltage, half at 0.974 V, from a tenth to nine tenths over 0.1 V.
// The filament's heating and cooling times are estimates.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

struct ExtPreampParams
{
    double rPot   = 1.0e6;   // R9 external input volume, 1 Mohm audio taper
    double rLoad  = 100.0e3; // R66: load on the pot wiper
    double gain   = 201.0;   // 1 + R61 / R62
    double vMax   = 9.8;     // Q33 saturation below the +10 V rail
    double vMin   = -7.7;    // R57 10k to -10 V against the 33k mixer load
    double fIn    = 15.9;    // C23 0.1 uF into R66 100k (Hz)
    double fFb    = 0.72;    // C26 220 uF with R62 1k (Hz)
    double fOut   = 4.8;     // C20 1 uF into the 33k mixer resistor (Hz)

    double vbe        = 0.53;    // Q25's drop at C14's holding current (ngspice)
    double tauCharge  = 50.0e-6; // C14 charging through Q25 (fast)
    double tauHold    = 0.367;   // C14 * (R56 + R48)
    double vHold      = -4.48;   // where C14 relaxes to: Q31's base current through R48 (ngspice)
    double lampHalf   = 0.974;   // C14's voltage for half the lamp's current (ngspice)
    double lampWidth  = 0.0235;  // the logistic's width (ngspice: 0.012 of full current rms)
    double tauHeat    = 0.025;   // filament heating (estimated)
    double tauCool    = 0.06;    // filament cooling (estimated)

    static ExtPreampParams drawn (Rng& rng, const ExtPreampParams& nominal) noexcept
    {
        ExtPreampParams p = nominal;
        p.rPot = rng.tolerance (nominal.rPot, 0.20);                       // carbon pot
        p.gain = 1.0 + rng.tolerance (nominal.gain - 1.0, 0.07);           // two 5 % resistors
        p.lampHalf = rng.tolerance (nominal.lampHalf, 0.10); // R56, R48 and Q31's gain
        return p;
    }
};

class ExtPreamp
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; update(); reset(); }
    void setParams (const ExtPreampParams& params) noexcept { p = params; update(); }

    /** EXTERNAL INPUT VOLUME knob, 0..1. */
    void setVolume (double pos) noexcept
    {
        // Audio-taper pot loaded by R66: Thevenin source a(1-a)Rpot into Rload.
        const double a = taper::audio (pos);
        const double rWiper = a * (1.0 - a) * p.rPot;
        potGain = a * p.rLoad / (p.rLoad + rWiper);
    }

    void reset() noexcept
    {
        hpIn = hpFb = hpOut = 0.0;
        prevIn = prevFb = prevOut = 0.0;
        vc14 = -p.vbe;
        heat = 0.0;
        lastOut = 0.0;
    }

    /** One sample: input in volts (jack), returns the signal sent to the mixer. */
    double process (double vin) noexcept
    {
        // Input coupling C23 / R66 (one-pole high-pass)
        const double x0 = potGain * vin;
        hpIn = aIn * (hpIn + x0 - prevIn);
        prevIn = x0;
        // Closed-loop gain 1 + 200 * s tau / (1 + s tau): C26 in the feedback leg
        hpFb = aFb * (hpFb + hpIn - prevFb);
        prevFb = hpIn;
        const double ideal = hpIn + (p.gain - 1.0) * hpFb;
        // Output stage saturation (sharp knee, asymmetric rails)
        const double v = clip (ideal);
        lastOut = v;
        lamp (v);
        // Output coupling C20 into the mixer
        hpOut = aOut * (hpOut + v - prevOut);
        prevOut = v;
        return hpOut;
    }

    /** Pre-amp output (before C20), volts. */
    double output() const noexcept { return lastOut; }

    /** Lamp filament heat, 0 (dark) .. 1 (full brightness). */
    double lampLevel() const noexcept { return heat; }

private:
    double clip (double y) const noexcept
    {
        const double lim = y >= 0.0 ? p.vMax : -p.vMin;
        const double r = y / lim;
        return y / std::sqrt (std::sqrt (1.0 + r * r * r * r));
    }

    void lamp (double v) noexcept
    {
        // C14 peak detector: charged quickly by Q25, relaxing through R56 + R48
        const double target = v - p.vbe;
        if (target > vc14)
            vc14 += (target - vc14) * kCharge;
        else
            vc14 = p.vHold + (vc14 - p.vHold) * kHold;
        vc14 = std::fmax (vc14, target);
        // Q31, Q34 and the lamp: the lamp's current against C14 (ngspice)
        const double i = 1.0 / (1.0 + std::exp (-(vc14 - p.lampHalf) / p.lampWidth));
        const double power = i * i;
        heat += (power - heat) * (power > heat ? kHeat : kCool);
    }

    void update() noexcept
    {
        auto hpCoeff = [this] (double f) { const double rc = 1.0 / (2.0 * M_PI * f); return rc / (rc + 1.0 / fs); };
        aIn = hpCoeff (p.fIn);
        aFb = hpCoeff (p.fFb);
        aOut = hpCoeff (p.fOut);
        kCharge = 1.0 - std::exp (-1.0 / (fs * p.tauCharge));
        kHold = std::exp (-1.0 / (fs * p.tauHold));
        kHeat = 1.0 - std::exp (-1.0 / (fs * p.tauHeat));
        kCool = 1.0 - std::exp (-1.0 / (fs * p.tauCool));
    }

    ExtPreampParams p;
    double fs = 192000.0, potGain = 0.0;
    double aIn = 0.0, aFb = 0.0, aOut = 0.0, kCharge = 0.0, kHold = 0.0, kHeat = 0.0, kCool = 0.0;
    double hpIn = 0.0, hpFb = 0.0, hpOut = 0.0, prevIn = 0.0, prevFb = 0.0, prevOut = 0.0;
    double vc14 = -0.6, heat = 0.0, lastOut = 0.0;
};

} // namespace mm
