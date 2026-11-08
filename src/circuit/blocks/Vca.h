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
// Voltage-controlled amplifier: differential pair whose tail current is set by
// the loudness contour through a V->I transistor, followed by a differential
// to single-ended op-amp stage.
//
//   Iout_diff = Itail(Vc) * tanh((Vin*a + d) / 2Vt)
//
// Modelled effects: input pair saturation (soft clipping of hot signals),
// pair mismatch d (control-voltage feedthrough: the classic VCA "thump"
// when the contour opens), V->I converter knee near cut-off (the VCA is
// never perfectly closed: a tiny bleed remains), and output stage limits.

#include "../components/OpAmp.h"
#include "../components/Passives.h"
#include "../components/Physics.h"

#include <cmath>

namespace mm
{

struct VcaParams
{
    double inputGain   = 0.010;   // filter output volts -> pair differential volts
    double offset      = 0.0;     // pair Vbe mismatch (V)
    double feedthrough = 0.004;   // common-mode rejection error of the output amp
    double iMax        = 1.0e-3;  // tail current at full contour
    double rLoad       = 10.0e3;  // collector load resistors
    double ampGain     = 0.5;     // differential amp gain (unity small-signal at full contour)
    double bleed       = 1.0e-4;  // fraction of iMax with contour at zero (80 dB range)

    static VcaParams drawn (Rng& rng, const VcaParams& nominal) noexcept
    {
        VcaParams p = nominal;
        p.offset = phys::thermalVoltage (298.15) * std::log (1.0 + 0.03 * rng.normal());
        p.feedthrough = nominal.feedthrough * (0.5 + rng.uniform());
        p.inputGain = rng.tolerance (nominal.inputGain, 0.05);
        p.bleed = nominal.bleed * (0.5 + rng.uniform());
        return p;
    }
};

class Vca
{
public:
    void prepare (double sampleRate) noexcept
    {
        amp.prepare (OpAmpParams::ua741(), sampleRate, 4.0);
        amp.reset();
    }
    void setParams (const VcaParams& params) noexcept { p = params; }
    void setTemperature (double kelvin) noexcept { inv2vt = 1.0 / (2.0 * phys::thermalVoltage (kelvin)); }

    /** `vin` = filter output (V), `control` = normalised contour 0..1. */
    double process (double vin, double control) noexcept
    {
        // V->I converter: soft knee near zero, never fully off.
        const double c = std::fmax (control, 0.0);
        const double iTail = p.iMax * (p.bleed + c * c / (c + 0.02) * (1.0 - p.bleed));
        const double diff = iTail * std::tanh ((vin * p.inputGain + p.offset) * inv2vt);
        // Differential output plus common-mode leak of the tail current.
        const double v = p.rLoad * (diff + p.feedthrough * iTail);
        return amp.process (p.ampGain * v);
    }

private:
    VcaParams p;
    OpAmpStage amp;
    double inv2vt = 1.0 / (2.0 * phys::thermalVoltage (298.15));
};

} // namespace mm
