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
// Contour (envelope) generator, board 2 circuit 8 (Fig. 9-7): a timing
// capacitor charged and discharged through the Attack / Decay pots by
// transistor switches, a peak detector that ends the attack, a sustain
// follower, and an output follower. Values are the capacitor's and the
// output's voltages, as on the board.
//
//   Attack : C charges through R42/R7 and the ATTACK pot towards the +9.3 V
//            rail (attack switch Q16/Q5 saturated). The flip-flop resets when
//            the output, divided by R24/R33 against R27/R29 16.9K to -10 V,
//            forward-biases CR6/CR3 (1N34A, germanium) into Q15/Q4's base:
//            at about 5.2 V (loudness) or 4.4 V (filter); "approximately 5
//            volts" (service manual 2.10).
//   Decay  : C discharges through the DECAY pot into the sustain node, held
//            by the PNP follower Q19/Q8 a Vbe above the SUSTAIN divider
//            (R19/R18 5K linear across +10 V, loaded by R57/R13 and R47, or
//            R1 and R17 for the filter section).
//   Release: gate off -> V-trig falls to 0.2 V and pulls the sustain node
//            down through CR9/CR2: C decays through the DECAY pot towards its
//            resting voltage (DECAY on), or is dumped through CR4/CR7 and
//            R1401 1.5K (DECAY off).
//   Output : the Darlington Q3/Q2 (loudness) or the complementary follower
//            Q22/Q21 (filter), a constant drop below the capacitor. The
//            loudness contour therefore rests at about -0.5 V, the filter
//            contour at about +0.15 V.
//
// Each RC segment is integrated exactly (exponential step), so the curve is
// independent of the sample rate. The trigger section that starts and ends
// it is TriggerSection. Not modelled: the electrolytic's dielectric
// absorption.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

struct ContourParams
{
    // Timing (Fig. 9-7, Table 7-3). Specifications: "RANGE OF ATTACK TIME: 10
    // milliseconds to 10 seconds", decay the same: 1 Mohm x 10 uF is 10 s.
    double c            = 10.0e-6; // timing capacitor: C5 (filter) / C2 (loudness), 10 uF electrolytic
    double rMin         = 100.0;   // attack series resistor: R7 (filter) / R42 (loudness)
    double rMinDecay    = 100.0;   // decay path at minimum setting: the sustain follower's R11 100 (Fig. 9-7)
    double rAttackPot   = 1.0e6;   // attack pot R12/R13: 1 Mohm audio taper (Table 7-3)
    double rDecayPot    = 1.0e6;   // decay pot R15/R16: 1 Mohm audio taper (Table 7-3)
    double rDump        = 1.5e3;   // R1401 1.5K (Fig. 9-12): the dump into V-trig with DECAY off
    double rSwitchOn    = 30.0;    // transistor switch saturation resistance

    // Levels (Fig. 9-7). The junction voltages are ngspice's on the drawn
    // circuit (tools/spice/contour.cir): the rest, the peak and the levels
    // held at six SUSTAIN settings, on the timing capacitor and at the output.
    double vRail        = 9.3;     // Q26's decoupled +9.3 V rail
    double vSwitchSat   = 0.1;     // attack switch saturation: the attack's target is the rail less this
    double vRest        = 0.645;   // capacitor at rest: V-trig (0.2 V) plus CR9/CR2 (ngspice)
    double vOutDrop     = 1.127;   // output follower: Darlington Q3/Q2, two Vbe (filter: Q22/Q21, 0.505 V);
                                   // ngspice, over the output's range (1.11 to 1.15 V; filter 0.49 to 0.52 V)
    double rPeak        = 6.8e3;   // peak detector divider: R24 6.8K (filter: R33 5.6K) ...
    double rPeakNeg     = 16.9e3;  // ... against R27/R29 16.9K 1 % to -10 V
    // Where the flip-flop resets: Q15/Q4 must draw the current that takes its
    // partner out of saturation, through the germanium CR6/CR3 (1N34A) and the
    // divider: ngspice's peaks, 5.22 V (filter: 4.43 V), give these.
    double vPeakJunctions = 0.855; // (filter: 0.842)
    double rSustainPot  = 5.0e3;   // SUSTAIN R19/R18: 5K linear across +10 V
    double rSustainSeries = 4.7e3; // R57 (filter: R13 4.7K) into the follower's base
    double rSustainGround = 4.7e3; // the 4.7K to ground labelled R41 (filter: R1 3K)
    double rSustainNeg  = 0.0;     // to -10 V: none (filter: R17 150K); 0 = absent
    // The sustain follower Q19/Q8: its Vbe, and its base current's load on the
    // divider as a gain, fitted to ngspice's six held levels (within 3 mV).
    double vSustainVbe  = 0.746;   // (filter: 0.739)
    double sustainGain  = 0.988;   // (filter: 0.990)

    /** The loudness section: R42, R24, R57, the 4.7K "R41", Darlington Q3/Q2. */
    static ContourParams loudness() noexcept { return {}; }

    /** The filter section: R7, R33 5.6K, R13 with R1 3K and R17 150K to -10 V,
        complementary follower Q22/Q21. */
    static ContourParams filter() noexcept
    {
        ContourParams p;
        p.rPeak = 5.6e3;
        p.rSustainGround = 3.0e3;
        p.rSustainNeg = 150.0e3;
        p.vOutDrop = 0.505;
        p.vPeakJunctions = 0.842;
        p.vSustainVbe = 0.739;
        p.sustainGain = 0.990;
        return p;
    }

    static ContourParams drawn (Rng& rng, const ContourParams& nominal) noexcept
    {
        ContourParams p = nominal;
        p.c          = rng.tolerance (nominal.c, 0.10, 0.50); // C2 / C5: "10uf, +50/-10%" (Table 7-6)
        p.rAttackPot = rng.tolerance (nominal.rAttackPot, 0.20); // carbon pots: 20 %
        p.rDecayPot  = rng.tolerance (nominal.rDecayPot, 0.20);
        p.rMin       = rng.tolerance (nominal.rMin, 0.05);
        p.rMinDecay  = rng.tolerance (nominal.rMinDecay, 0.05);
        p.rPeak      = rng.tolerance (nominal.rPeak, 0.05);
        p.rPeakNeg   = rng.tolerance (nominal.rPeakNeg, 0.01);
        p.rDump      = rng.tolerance (nominal.rDump, 0.05);
        p.vRail      = rng.tolerance (nominal.vRail, 0.02);
        return p;
    }

    /** Output voltage at which the peak detector resets the flip-flop. */
    double peakOutput() const noexcept
    {
        return (vPeakJunctions * (rPeak + rPeakNeg) + 10.0 * rPeak) / rPeakNeg;
    }

    /** Capacitor voltage the decay settles at for a SUSTAIN position 0..1. */
    double sustainCap (double pos) const noexcept
    {
        const double s = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
        // Thevenin of the pot's wiper, then the base node by Millman.
        const double vTh = 10.0 * s;
        const double rTh = rSustainPot * s * (1.0 - s);
        double g = 1.0 / (rSustainSeries + rTh) + 1.0 / rSustainGround;
        double i = vTh / (rSustainSeries + rTh);
        if (rSustainNeg > 0.0) { g += 1.0 / rSustainNeg; i -= 10.0 / rSustainNeg; }
        return sustainGain * i / g + vSustainVbe;
    }
};

class ContourGen
{
public:
    enum class Stage { Idle, Attack, Decay, Release };

    void prepare (double sampleRate) noexcept { h = 1.0 / sampleRate; update(); }
    void setParams (const ContourParams& params) noexcept { p = params; update(); }

    /** Knob positions 0..1 (panel 0..10). */
    void setAttack (double pos) noexcept { attackPos = pos; update(); }
    void setDecay (double pos) noexcept { decayPos = pos; update(); }
    void setSustain (double pos) noexcept { sustainPos = pos; update(); }
    void setDecaySwitch (bool on) noexcept { decaySwitch = on; }

    void gateOn() noexcept { stage = Stage::Attack; }
    void gateOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset() noexcept { v = p.vRest; stage = Stage::Idle; }

    /** One sample; returns the contour's output voltage. */
    double process() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                v = vTarget + (v - vTarget) * kAttack;
                if (v >= vTrip)
                    stage = Stage::Decay;
                break;
            case Stage::Decay:
                v = vSustain + (v - vSustain) * kDecay;
                break;
            case Stage::Release:
                v = p.vRest + (v - p.vRest) * (decaySwitch ? kDecay : kRelease);
                if (v - p.vRest < 1.0e-5) { v = p.vRest; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }
        return output();
    }

    /** The contour's output voltage (the follower below the capacitor). */
    double output() const noexcept { return v - p.vOutDrop; }
    /** The timing capacitor's voltage. */
    double value() const noexcept { return v; }
    Stage currentStage() const noexcept { return stage; }
    const ContourParams& params() const noexcept { return p; }

    /** Time constant (s) of the attack path for the current setting. */
    double attackTau() const noexcept { return tauA; }
    double decayTau() const noexcept { return tauD; }

private:
    void update() noexcept
    {
        tauA = (p.rMin + p.rSwitchOn + attackPot (p, attackPos)) * p.c;
        tauD = (p.rMinDecay + p.rSwitchOn + decayPot (p, decayPos)) * p.c;
        const double tauR = (p.rDump + p.rSwitchOn) * p.c;
        kAttack = std::exp (-h / tauA);
        kDecay = std::exp (-h / tauD);
        kRelease = std::exp (-h / tauR);
        vTarget = p.vRail - p.vSwitchSat;
        vTrip = p.peakOutput() + p.vOutDrop;
        vSustain = std::fmin (p.sustainCap (sustainPos), vTrip);
    }

public:
    /** ATTACK / DECAY pot in circuit (ohm): R12 / R13, R15 / R16, 1 Mohm audio
        rheostats (Table 7-3), with this unit's pot tolerance. */
    static double attackPot (const ContourParams& p, double pos) noexcept
    {
        return p.rAttackPot * taper::audio (pos);
    }
    static double decayPot (const ContourParams& p, double pos) noexcept
    {
        return p.rDecayPot * taper::audio (pos);
    }

    /** Ratio of the time from rest to the peak to the attack's time constant. */
    static double attackSpan (const ContourParams& p) noexcept
    {
        const double target = p.vRail - p.vSwitchSat;
        return std::log ((target - p.vRest) / (target - (p.peakOutput() + p.vOutDrop)));
    }

    /** ATTACK TIME: time from gate to the peak, from rest (s). */
    static double attackTime (const ContourParams& p, double pos) noexcept
    {
        return (p.rMin + p.rSwitchOn + attackPot (p, pos)) * p.c * attackSpan (p);
    }

    /** DECAY TIME read on the panel: time constant of the decay (s). */
    static double decayTime (const ContourParams& p, double pos) noexcept
    {
        return (p.rMinDecay + p.rSwitchOn + decayPot (p, pos)) * p.c;
    }

private:
    ContourParams p;
    Stage stage = Stage::Idle;
    double h = 1.0 / 48000.0, v = 0.66;
    double attackPos = 0.0, decayPos = 0.3, sustainPos = 1.0;
    bool decaySwitch = false;
    double tauA = 0.001, tauD = 0.1, kAttack = 0.0, kDecay = 0.0, kRelease = 0.0;
    double vTarget = 9.2, vTrip = 6.9, vSustain = 5.6;
};

} // namespace mm
