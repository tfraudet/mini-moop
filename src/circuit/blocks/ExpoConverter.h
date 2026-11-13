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
// The oscillators' exponential (V/oct -> current) converter, built around a
// matched NPN pair.
//
// Target revision: Oscillator Board No. 1, revision 2 ("old board", serial
// numbers 1237 to 10174, service manual Fig. 9-3): RCA CA3046 transistor
// array (IC2) at ambient temperature, compensated by a 1 kohm +3500 ppm/K
// tempco resistor (R20) in the feedback of the 741 CV summer IC1, keyboard
// input R27 = 51.1 kohm 1 %. The reference transistor's collector current is
// set by R50 = 39 kohm from ground, with its collector held at -4 V by IC3.
// Not modelled: revision 1 (discrete 2N4058 PNP pairs, no tempco) and
// revision 3 (heated uA726 at 80 degC, serial 10175 and above), nor the
// 94-001 modification kit.
//
//   Iout = Is_out * exp((Vbe_ref + dVbe - Iout*rE) / Vt)
//   Vbe_ref = Vt * ln(Iref / Is_ref) + Iref*rE
//   dVbe = Vcv * Rtc(T_tc) / (Rin + Rtc(T_tc))        (input attenuator)
//
// Modelled physical effects:
//   * Vt = kT/q at the pair's temperature (the reason the Minimoog drifts)
//   * +3500 ppm/K tempco resistor at a slightly different temperature
//   * Is mismatch of the "matched" pair (pitch offset)
//   * the pair's emitter and base resistance (with R7 100 in the output
//     transistor's base), which flattens the high notes, and the fixed
//     compensation the board has for it: R42 160K from the tail transistor's
//     emitter (R79 1K) back to the summing node, "positive feedback around
//     IC1" (service manual 2.16), worth R79 R27 / R42 volts per ampere. The
//     old board has no trimmer for it: only RANGE (R11) and SCALE (R8)
//   * the RANGE and SCALE trimmers, as offset and scale

#include "../components/Passives.h"
#include "../components/Physics.h"

#include <cmath>

namespace mm
{

struct ExpoConverterParams
{
    double iRef       = 4.0 / 39.0e3; // reference current: -4 V across R50 = 39 kohm (~103 uA)
    double isRef      = 6.734e-15; // saturation current of reference transistor
    double isMismatch = 1.0;       // Is_out / Is_ref
    double rE         = 4.7;       // the pair's emitter resistance and base resistance over beta, with R7 (ohm):
                                   // taken as what R42's compensation, less the reset time's share, cancels
    double rIn        = 51.1e3;    // CV input resistor (R27 keyboard input, 51.1 kohm 1 %)
    double rTc        = 1.0e3;     // tempco resistor R20 at 25 °C
    double tcPpm      = 3500.0;    // tempco coefficient
    double hfTrim     = 1.0e3 * 51.1e3 / 160.0e3; // R42's compensation: R79 R27 / R42, V of CV per A of Iout

    /** Draws component tolerances (1 % metal film input resistors, 5 % carbon
        R50, transistor pair matched to about 2 % in Is, emitter resistance spread). */
    static ExpoConverterParams drawn (Rng& rng, const ExpoConverterParams& nominal) noexcept
    {
        ExpoConverterParams p = nominal;
        p.isMismatch = 1.0 + 0.01 * rng.normal();
        p.rE   = rng.tolerance (nominal.rE, 0.15);
        p.hfTrim = rng.tolerance (nominal.hfTrim, 0.07); // R79 and R42, 5 % each
        p.rIn  = rng.tolerance (nominal.rIn, 0.01);
        p.rTc  = rng.tolerance (nominal.rTc, 0.05);
        p.iRef = rng.tolerance (nominal.iRef, 0.05);
        return p;
    }
};

class ExpoConverter
{
public:
    void setParams (const ExpoConverterParams& params) noexcept { p = params; update(); }
    const ExpoConverterParams& params() const noexcept { return p; }

    /** Temperatures (K): reference transistor, output transistor (a discrete
        "matched" pair is never perfectly isothermal) and tempco resistor. */
    void setTemperatures (double refK, double outK, double tempcoK) noexcept
    {
        tRef = refK;
        tOut = outK;
        tTc = tempcoK;
        update();
    }

    /** Trimmers: `scaleTrim` multiplies the attenuator ratio (volts/octave
        trim), `offsetV` is added to the control voltage (tuning trim). */
    void setTrims (double scaleTrim, double offsetV) noexcept { scale = scaleTrim; offset = offsetV; update(); }
    double scaleTrim() const noexcept { return scale; }
    void setHfTrim (double v) noexcept { p.hfTrim = v; stale = true; }
    double hfTrim() const noexcept { return p.hfTrim; }
    double attenuatorRatio() const noexcept { return ratio; }
    double offsetTrim() const noexcept { return offset; }

    /** Output collector current for the given control voltage (1 V/oct domain). */
    double current (double vcv) noexcept
    {
        if (! stale && std::abs (vcv - lastCv) <= 0.0) // exact repeat: reuse
            return lastI;
        const double dVbe = (vcv + offset) * ratio;
        // Solve I = IsOut * exp((vbeRef + dVbe + hf*I - I*rE)/Vt) in the log
        // domain by Newton, warm-started from the previous solution (one or
        // two iterations in practice).
        const double rNet = p.rE - p.hfTrim * ratio;
        const double a = std::fmin ((vbeRef + dVbe) / vt, 60.0) + lnIsOut;
        double lnI = stale ? a : lastLnI;
        double i = std::exp (lnI);
        for (int k = 0; k < 6; ++k)
        {
            // f(lnI) = lnI - a + I*rNet/Vt
            const double e = i * rNet / vt;
            const double f = lnI - a + e;
            const double step = f / (1.0 + e);
            lnI -= step;
            i = std::exp (lnI);
            if (std::abs (step) < 1.0e-10)
                break;
        }
        lastCv = vcv;
        lastLnI = lnI;
        lastI = i;
        stale = false;
        return i;
    }

    /** Ideal output current for the given CV (no rE, nominal scale):
        Iref * 2^(vcv). Useful to compute calibration targets. */
    double referenceCurrent() const noexcept { return p.iRef; }

    double thermalVoltage() const noexcept { return vt; }

private:
    void update() noexcept
    {
        stale = true;
        constexpr double tNom = 298.15;
        // SPICE temperature law for the saturation current (XTI = 3, EG = 1.11 eV)
        auto isAt = [] (double is, double t) {
            const double vtT = phys::thermalVoltage (t);
            return is * std::pow (t / tNom, 3.0) * std::exp (1.11 / vtT * (t / tNom - 1.0));
        };
        vt = phys::thermalVoltage (tOut);
        const double vtRef = phys::thermalVoltage (tRef);
        const double rTcT = p.rTc * (1.0 + p.tcPpm * 1.0e-6 * (tTc - tNom));
        // Attenuator ratio; the trimmer (scale) sets it to Vt*ln2 per volt.
        ratio = scale * rTcT / (p.rIn + rTcT);
        const double iRefT = p.iRef * (1.0 + 100.0e-6 * (tTc - tNom)); // reference resistor drift
        isOut = isAt (p.isRef * p.isMismatch, tOut);
        lnIsOut = std::log (isOut);
        vbeRef = vtRef * std::log (iRefT / isAt (p.isRef, tRef)) + iRefT * p.rE;
    }

    ExpoConverterParams p;
    double tRef = 298.15, tOut = 298.15, tTc = 298.15;
    double scale = 1.0, offset = 0.0;
    double vt = 0.0257, ratio = 0.0, isOut = 1e-14, lnIsOut = 0.0, vbeRef = 0.6;
    double lastCv = 0.0, lastLnI = 0.0, lastI = 0.0;
    bool stale = true;
};

} // namespace mm
