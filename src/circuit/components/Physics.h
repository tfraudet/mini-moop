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
// Physical constants and numerically-safe helpers shared by all device models.

#include <algorithm>
#include <cmath>

namespace mm::phys
{
inline constexpr double kBoltzmann = 1.380649e-23;   // J/K
inline constexpr double qElectron  = 1.602176634e-19; // C
inline constexpr double kelvin0C   = 273.15;
inline constexpr double roomTempK  = 300.15;           // 27 °C, SPICE TNOM default

/** Thermal voltage kT/q in volts. */
inline constexpr double thermalVoltage (double tempK) noexcept
{
    return kBoltzmann * tempK / qElectron;
}

/** exp() with linear continuation above `limit`, as SPICE does, so Newton
    iterations can never overflow when a junction is grossly forward-biased. */
inline double limexp (double x, double limit = 80.0) noexcept
{
    if (x < limit)
        return std::exp (x);
    const double e = std::exp (limit);
    return e * (1.0 + (x - limit));
}

/** Derivative of limexp. */
inline double limexpDeriv (double x, double limit = 80.0) noexcept
{
    return x < limit ? std::exp (x) : std::exp (limit);
}

/** SPICE3 pnjlim: limits the change of a junction voltage between Newton
    iterations so the exponential stays in a region where linearisation is valid. */
inline double pnjlim (double vnew, double vold, double vt, double vcrit) noexcept
{
    if (vnew > vcrit && std::abs (vnew - vold) > 2.0 * vt)
    {
        if (vold > 0.0)
        {
            const double arg = 1.0 + (vnew - vold) / vt;
            vnew = arg > 0.0 ? vold + vt * std::log (arg) : vcrit;
        }
        else
        {
            vnew = vt * std::log (vnew / vt);
        }
    }
    return vnew;
}

inline double criticalVoltage (double is, double vt) noexcept
{
    return vt * std::log (vt / (std::sqrt (2.0) * is));
}

} // namespace mm::phys
