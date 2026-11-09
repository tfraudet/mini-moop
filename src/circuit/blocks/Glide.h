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
// Glide (portamento): the keyboard CV is smoothed by an RC lag (glide pot +
// series resistor into a capacitor), buffered by an op-amp follower. With the
// GLIDE switch off the pot is bypassed and only the small series resistance
// remains. Exponential (constant time) glide, as on the original.

#include "../components/Passives.h"

#include <cmath>

namespace mm
{

struct GlideParams
{
    double c       = 1.0e-6;   // glide capacitor C6, 1 uF (Board 2, service manual Fig. 9-7)
    double rSeries = 1.0e3;    // minimum series resistance (switch off / pot at 0)
    double rPot    = 5.0e6;    // glide pot R2: 5 Mohm "No. 1 taper" (Table 7-3), up to ~5 s time constant;
                               // the No. 1 curve is not documented, modelled as audio taper
    double leakage = 1.0e-10;  // capacitor/FET leakage (V/s on 1 uF -> slow sag)

    static GlideParams drawn (Rng& rng, const GlideParams& nominal) noexcept
    {
        GlideParams p = nominal;
        p.c = rng.tolerance (nominal.c, 0.10);
        p.rPot = rng.tolerance (nominal.rPot, 0.20);
        return p;
    }
};

class Glide
{
public:
    void prepare (double sampleRate) noexcept { h = 1.0 / sampleRate; update(); }
    void setParams (const GlideParams& params) noexcept { p = params; update(); }
    void setTime (double pos) noexcept { pot = pos; update(); }
    void setEnabled (bool on) noexcept { enabled = on; update(); }
    void reset (double v) noexcept { y = v; }

    double process (double target) noexcept
    {
        y = target + (y - target) * k;
        return y;
    }

    double value() const noexcept { return y; }

private:
    void update() noexcept
    {
        const double r = p.rSeries + (enabled ? p.rPot * taper::audio (pot) : 0.0);
        k = std::exp (-h / (r * p.c));
    }

    GlideParams p;
    double h = 1.0 / 48000.0, pot = 0.0, k = 0.0, y = 0.0;
    bool enabled = false;
};

} // namespace mm
