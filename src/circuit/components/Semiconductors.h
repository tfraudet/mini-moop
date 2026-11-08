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
// Junction device models: Shockley diode and Gummel-Poon-lite BJT
// (transport Ebers-Moll + forward Early effect), formulated exactly as SPICE
// does when only IS, BF, BR, VAF are given, so the models can be validated
// against ngspice.

#include "Physics.h"

namespace mm
{

struct DiodeParams
{
    double is = 2.52e-9; // 1N4148-ish
    double n  = 1.752;
};

struct DiodeEval
{
    double i = 0.0; // current anode -> cathode
    double g = 0.0; // di/dv
};

inline DiodeEval evalDiode (const DiodeParams& p, double v, double vt) noexcept
{
    const double nvt = p.n * vt;
    const double e = phys::limexp (v / nvt);
    return { p.is * (e - 1.0), p.is * phys::limexpDeriv (v / nvt) / nvt };
}

struct BjtParams
{
    double is  = 1.0e-14;
    double bf  = 250.0;
    double br  = 4.0;
    double vaf = 100.0;   // forward Early voltage; <= 0 disables
    bool   pnp = false;

    /** Typical small-signal NPN of the era (2N3392 / 2N3904 class). */
    static BjtParams smallSignalNpn() noexcept { return { 6.734e-15, 220.0, 0.7371, 74.03, false }; }
    /** Typical small-signal PNP (2N3906 class). */
    static BjtParams smallSignalPnp() noexcept { return { 1.41e-15, 180.7, 4.977, 18.7, true }; }
};

/** Terminal currents (into the device) and their partial derivatives with
    respect to the junction voltages vbe, vbc (in the device's own polarity). */
struct BjtEval
{
    double ic = 0.0, ib = 0.0;
    double dic_dvbe = 0.0, dic_dvbc = 0.0;
    double dib_dvbe = 0.0, dib_dvbc = 0.0;
};

/** Evaluate an NPN-polarity transistor. For PNP callers flip the junction
    voltages and the resulting currents (done by BjtDevice in the MNA). */
inline BjtEval evalBjt (const BjtParams& p, double vbe, double vbc, double vt, double gmin = 1.0e-12) noexcept
{
    const double ebe = phys::limexp (vbe / vt);
    const double ebc = phys::limexp (vbc / vt);
    const double cbe = p.is * (ebe - 1.0) + gmin * vbe;
    const double gbe = p.is * phys::limexpDeriv (vbe / vt) / vt + gmin;
    const double cbc = p.is * (ebc - 1.0) + gmin * vbc;
    const double gbc = p.is * phys::limexpDeriv (vbc / vt) / vt + gmin;

    // Early effect: 1/qb = 1 - vbc/VAF (SPICE GP with VAR = inf, IKF = inf)
    const double invqb = p.vaf > 0.0 ? (1.0 - vbc / p.vaf) : 1.0;
    const double dinvqb = p.vaf > 0.0 ? (-1.0 / p.vaf) : 0.0;

    const double cce = cbe - cbc;

    BjtEval r;
    r.ic = cce * invqb - cbc / p.br;
    r.ib = cbe / p.bf + cbc / p.br;
    r.dic_dvbe = gbe * invqb;
    r.dic_dvbc = -gbc * invqb + cce * dinvqb - gbc / p.br;
    r.dib_dvbe = gbe / p.bf;
    r.dib_dvbc = gbc / p.br;
    return r;
}

} // namespace mm
