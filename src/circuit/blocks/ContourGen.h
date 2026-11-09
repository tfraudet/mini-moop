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
// Contour (envelope) generator: a timing capacitor charged and discharged
// through the Attack / Decay pots by transistor switches, with a comparator
// that ends the attack phase.
//
//   Attack : C charges through (R_attack_pot + R_min) towards V_charge
//            (above the comparator threshold, giving the typical
//            "truncated exponential" attack).
//   Decay  : at the threshold the comparator flips, C discharges through
//            (R_decay_pot + R_min) towards the Sustain pot voltage.
//   Release: gate off -> C discharges towards 0 V through the Decay pot when
//            the DECAY switch is on, otherwise through a short fixed path.
//
// Each RC segment is integrated exactly (exponential step), so the curve is
// independent of the sample rate.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

struct ContourParams
{
    // Board 2 (93-114), service manual Fig. 9-7 and Table 7-3. Spec ranges:
    // attack 1 ms..10 s, decay 4 ms..35 s (decay time read as ~3 time constants).
    // With C = 10 uF and R7 = 100 ohm, a 1 Mohm attack pot gives 1.3 ms..9.8 s
    // to the 5 V threshold, matching the spec.
    double c            = 10.0e-6; // timing capacitor: C5 (filter) / C2 (loudness), 10 uF electrolytic
    double rMin         = 100.0;   // attack series resistor: R7 (filter) / R42 (loudness)
    double rMinDecay    = 100.0;   // decay path at minimum setting (assumed: from the 4 ms spec)
    double rAttackPot   = 1.0e6;   // attack pot R12/R13: 1 Mohm audio taper (Table 7-3)
    double rDecayPot    = 1.0e6;   // decay pot R15/R16: 1 Mohm audio taper (Table 7-3)
    double rRelease     = 330.0;   // release path when DECAY switch is off (assumed: ~3 ms)
    double rSwitchOn    = 30.0;    // transistor switch saturation resistance
    double vCharge      = 8.0;     // attack target voltage
    double vThreshold   = 5.0;     // comparator threshold = peak of the contour
    double hysteresis   = 0.02;    // comparator hysteresis (V)

    // Attack / decay pot law, calibrated on the panel legend (10, 200, 600 ms,
    // 1, 5, 10 s): a carbon audio taper made of two linear segments with 10 %
    // of the track at the knee, whose electrical travel covers the dial from
    // 7.5 % to 89 % (the knee falls at 61.5 % of the dial). The legend then
    // reads the time constant of the timing network (attack: time to the peak).
    double dialStart    = 0.075;   // dial position where the track begins
    double dialKnee     = 0.615;   // dial position of the taper knee
    double dialEnd      = 0.89;    // dial position where the track ends
    double kneeFraction = 0.10;    // fraction of the track resistance at the knee
    double leakage      = 5.0e-8;  // electrolytic capacitor + switch leakage current (A, estimated)

    static ContourParams drawn (Rng& rng, const ContourParams& nominal) noexcept
    {
        ContourParams p = nominal;
        p.c          = rng.tolerance (nominal.c, 0.20);   // aluminium electrolytic: 20 %
        p.rAttackPot = rng.tolerance (nominal.rAttackPot, 0.20); // carbon pots: 20 %
        p.rDecayPot  = rng.tolerance (nominal.rDecayPot, 0.20);
        p.rMin       = rng.tolerance (nominal.rMin, 0.05);
        p.rMinDecay  = rng.tolerance (nominal.rMinDecay, 0.05);
        p.vThreshold = rng.tolerance (nominal.vThreshold, 0.02);
        p.vCharge    = rng.tolerance (nominal.vCharge, 0.02);
        return p;
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
    void reset() noexcept { v = 0.0; stage = Stage::Idle; }

    /** One sample; returns the capacitor voltage (0 .. ~vThreshold). */
    double process() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                v = vCharge + (v - vCharge) * kAttack;
                if (v >= p.vThreshold)
                {
                    v = p.vThreshold;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                v = vSustain + (v - vSustain) * kDecay;
                break;
            case Stage::Release:
                v *= decaySwitch ? kDecay : kRelease;
                if (v < 1.0e-5) { v = 0.0; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                v = std::fmax (0.0, v - p.leakage / p.c * h);
                break;
        }
        return v;
    }

    double value() const noexcept { return v; }
    double normalised() const noexcept { return v / p.vThreshold; }
    Stage currentStage() const noexcept { return stage; }

    /** Time constant (s) of the attack path for the current setting. */
    double attackTau() const noexcept { return tauA; }
    double decayTau() const noexcept { return tauD; }

private:
    void update() noexcept
    {
        tauA = (p.rMin + p.rSwitchOn + p.rAttackPot * track (p, attackPos)) * p.c;
        tauD = (p.rMinDecay + p.rSwitchOn + p.rDecayPot * track (p, decayPos)) * p.c;
        const double tauR = (p.rRelease + p.rSwitchOn) * p.c;
        kAttack = std::exp (-h / tauA);
        kDecay = std::exp (-h / tauD);
        kRelease = std::exp (-h / tauR);
        vCharge = p.vCharge;
        // Sustain pot: linear divider of the peak voltage
        vSustain = p.vThreshold * sustainPos;
    }

public:
    /** Fraction of the attack / decay pot resistance for a dial position 0..1. */
    static double track (const ContourParams& p, double pos) noexcept
    {
        if (pos <= p.dialStart) return 0.0;
        if (pos < p.dialKnee) return p.kneeFraction * (pos - p.dialStart) / (p.dialKnee - p.dialStart);
        if (pos < p.dialEnd) return p.kneeFraction + (1.0 - p.kneeFraction) * (pos - p.dialKnee) / (p.dialEnd - p.dialKnee);
        return 1.0;
    }

    /** ATTACK TIME read on the panel: time from gate to the peak (s). */
    static double attackTime (const ContourParams& p, double pos) noexcept
    {
        const double tau = (p.rMin + p.rSwitchOn + p.rAttackPot * track (p, pos)) * p.c;
        return tau * std::log (p.vCharge / (p.vCharge - p.vThreshold));
    }

    /** DECAY TIME read on the panel: time constant of the decay (s). */
    static double decayTime (const ContourParams& p, double pos) noexcept
    {
        return (p.rMinDecay + p.rSwitchOn + p.rDecayPot * track (p, pos)) * p.c;
    }

private:
    ContourParams p;
    Stage stage = Stage::Idle;
    double h = 1.0 / 48000.0, v = 0.0;
    double attackPos = 0.0, decayPos = 0.3, sustainPos = 1.0;
    bool decaySwitch = false;
    double tauA = 0.001, tauD = 0.1, kAttack = 0.0, kDecay = 0.0, kRelease = 0.0;
    double vCharge = 8.0, vSustain = 5.0;
};

} // namespace mm
