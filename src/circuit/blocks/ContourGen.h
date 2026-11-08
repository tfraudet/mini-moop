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
    // Ranges from the service manual spec: attack 1 ms..10 s, decay 4 ms..35 s
    // (decay time read as ~3 time constants).
    double c            = 2.2e-6;  // timing capacitor (F)
    double rMin         = 470.0;   // attack series resistance at minimum setting (ohm)
    double rMinDecay    = 680.0;   // decay series resistance at minimum setting (ohm)
    double rAttackPot   = 5.0e6;   // attack pot (5 Mohm, log taper)
    double rDecayPot    = 5.0e6;   // decay pot (5 Mohm, log taper)
    double rRelease     = 1.5e3;   // release path when DECAY switch is off (ohm)
    double rSwitchOn    = 30.0;    // transistor switch saturation resistance
    double vCharge      = 8.0;     // attack target voltage
    double vThreshold   = 5.0;     // comparator threshold = peak of the contour
    double hysteresis   = 0.02;    // comparator hysteresis (V)
    double leakage      = 2.0e-9;  // capacitor + switch leakage current (A)

    static ContourParams drawn (Rng& rng, const ContourParams& nominal) noexcept
    {
        ContourParams p = nominal;
        p.c          = rng.tolerance (nominal.c, 0.10);
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
        tauA = (p.rMin + p.rSwitchOn + p.rAttackPot * taper::audio (attackPos)) * p.c;
        tauD = (p.rMinDecay + p.rSwitchOn + p.rDecayPot * taper::audio (decayPos)) * p.c;
        const double tauR = (p.rRelease + p.rSwitchOn) * p.c;
        kAttack = std::exp (-h / tauA);
        kDecay = std::exp (-h / tauD);
        kRelease = std::exp (-h / tauR);
        vCharge = p.vCharge;
        // Sustain pot: linear divider of the peak voltage
        vSustain = p.vThreshold * sustainPos;
    }

    ContourParams p;
    Stage stage = Stage::Idle;
    double h = 1.0 / 48000.0, v = 0.0;
    double attackPos = 0.0, decayPos = 0.3, sustainPos = 1.0;
    bool decaySwitch = false;
    double tauA = 0.001, tauD = 0.1, kAttack = 0.0, kDecay = 0.0, kRelease = 0.0;
    double vCharge = 8.0, vSustain = 5.0;
};

} // namespace mm
