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
// The contours' trigger section (board 2, circuit 8, Fig. 9-7): the keyboard's
// trigger bus (+10 V while a key is down, -10 V otherwise) reaches Q20's base
// through the germanium CR10, R60 100K into C7 0.1 uF, and R55 47K. Q20 on
// turns Q12 off, and Q12's collector, on R23 3.3K, is the V-trigger that
// starts both contours. C7 delays both edges:
//
//   key down  C7 charges through R60 until Q20 draws enough base current to
//             saturate. Between notes the germanium diode's reverse leakage
//             lets C7 drift down towards the bus's -10 V, so after a rest the
//             charge starts from there: about 8 ms. A key struck again soon
//             after a release finds C7 near Q20's Vbe: well under a
//             millisecond.
//   key up    CR10 blocks, and C7 only empties into Q20's base through R55:
//             the V-trigger holds for about 11 ms after the release.
//
// ngspice on the drawn circuit (tools/spice/trigger.cir), C7 starting at
// -10 V: 7.9 ms and 10.8 ms. The pitch is sampled without this delay (the
// keyboard circuit's own gate, R65 5.1K and C13 .01 uF: 51 us).

#include <algorithm>

namespace mm
{

struct TriggerParams
{
    double r60 = 100.0e3;      // R60: from the bus through CR10
    double r55 = 47.0e3;       // R55: into Q20's base
    double c7 = 0.1e-6;        // C7
    double vBusOn = 9.75;      // the bus less CR10's (germanium) drop
    double vBusOff = -10.0;
    // Q20's base-emitter junction, and the base current that saturates it
    // (vOn / R55, about 4 uA): the two that give ngspice's two delays, with the
    // leakage below.
    double vbe = 0.62;
    double vOn = 0.185;
    double leakage = 2.6e-6;   // CR10's reverse leakage, A (the 1N34A card of tools/spice/models.lib)
};

class TriggerSection
{
public:
    void prepare (double sampleRate) noexcept { h = 1.0 / sampleRate; }
    void setParams (const TriggerParams& params) noexcept { p = params; }

    /** Between notes, after a long rest: C7 at the bus's -10 V. */
    void reset() noexcept { vc = p.vBusOff; on = false; }

    /** One sample with the key state; returns whether the V-trigger is high. */
    bool process (bool keyDown) noexcept
    {
        // C7's current: in from the bus through R60 while a key is down (CR10
        // conducting), out through the leaking CR10 otherwise; out into Q20's
        // base through R55 above its Vbe.
        double i = keyDown ? (p.vBusOn - vc) / p.r60 : (vc > p.vBusOff ? -p.leakage : 0.0);
        if (vc > p.vbe)
            i -= (vc - p.vbe) / p.r55;
        vc = std::max (p.vBusOff, vc + i * h / p.c7);
        on = vc > p.vbe + p.vOn;
        return on;
    }

    bool isOn() const noexcept { return on; }
    double c7Volts() const noexcept { return vc; }

private:
    TriggerParams p;
    double h = 1.0 / 48000.0, vc = -10.0;
    bool on = false;
};

} // namespace mm
