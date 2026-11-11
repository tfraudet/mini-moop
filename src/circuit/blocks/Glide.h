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
// Glide (portamento), board 2 circuit 1 (Fig. 9-7): the keyboard's hold
// amplifier charges C6 1 uF through the GLIDE pot (R2, 5M). A key change sends
// the amplifier into saturation towards one of its rails, so C6 charges
// through the pot from a nearly constant voltage: the pitch slides in a
// straight line (constant octaves per second), faster up than down, and
// settles exponentially once the amplifier is back in its linear range. With
// the GLIDE switch off the pot is shorted and only the small series
// resistance remains.
//
// The pot's law and the two drive voltages come from the CA-72 project's
// hardware reference (its session J: C3-C4-C3 at GLIDE 2.5, 5, 7.5 and 10,
// 20-80 % of the octave at 61/22, 29/18, 3.8/2.1 and 1.6/0.9 octaves a second
// up/down): 162K, 257K, 2.1M and 5M at those positions (below 2.5 the audio
// taper's shape scaled to meet 162K), and 8.0 V up / 4.45 V down, the drives
// that give the measured rates through those resistances at 7.5 and 10.

#include "../components/Passives.h"

#include <array>
#include <cmath>
#include <utility>

namespace mm
{

struct GlideParams
{
    double c       = 1.0e-6;   // glide capacitor C6, 1 uF (Board 2, service manual Fig. 9-7)
    double rSeries = 1.0e3;    // minimum series resistance (switch off / pot at 0)
    double rPot    = 5.0e6;    // glide pot R2: 5 Mohm (Table 7-3), the law below scaled by its tolerance
    double vUp     = 8.0;      // hold amplifier's drive across the pot while saturated, rising (V)
    double vDown   = 4.45;     // ... falling (V)
    double leakage = 1.0e-10;  // capacitor/FET leakage (V/s on 1 uF -> slow sag)
    std::array<std::pair<double, double>, 4> law { { { 0.25, 162.0e3 }, { 0.5, 257.0e3 }, { 0.75, 2.1e6 }, { 1.0, 5.0e6 } } };

    static GlideParams drawn (Rng& rng, const GlideParams& nominal) noexcept
    {
        GlideParams p = nominal;
        p.c = rng.tolerance (nominal.c, 0.10);
        p.rPot = rng.tolerance (nominal.rPot, 0.20);
        return p;
    }

    /** GLIDE pot resistance (ohm) at a knob position 0..1, nominal 5M pot. */
    double potAt (double pos) const noexcept
    {
        const double x = pos < 0.0 ? 0.0 : pos > 1.0 ? 1.0 : pos;
        auto audio = [] (double t) { return (std::pow (81.0, t) - 1.0) / 80.0; };
        if (x <= law[0].first)
            return law[0].second * audio (x) / audio (law[0].first);
        for (size_t i = 1; i < law.size(); ++i)
            if (x <= law[i].first)
            {
                const auto& a = law[i - 1];
                const auto& b = law[i];
                return a.second * std::pow (b.second / a.second, (x - a.first) / (b.first - a.first));
            }
        return law.back().second;
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

    /** `target`: the keyboard's CV (1 V/octave). */
    double process (double target) noexcept
    {
        // Linear range: the amplifier's loop through the series resistance.
        double step = (target - y) * (1.0 - kLinear);
        // Saturated: C6 charged through the pot from the rail's drive.
        step = std::fmin (std::fmax (step, -maxDown), maxUp);
        y += step;
        return y;
    }

    double value() const noexcept { return y; }

private:
    void update() noexcept
    {
        const double r = p.rSeries + (enabled ? p.potAt (pot) * (p.rPot / 5.0e6) : 0.0);
        kLinear = std::exp (-h / (p.rSeries * p.c));
        maxUp = p.vUp / (r * p.c) * h;
        maxDown = p.vDown / (r * p.c) * h;
    }

    GlideParams p;
    double h = 1.0 / 48000.0, pot = 0.0, kLinear = 0.0, maxUp = 1.0, maxDown = 1.0, y = 0.0;
    bool enabled = false;
};

} // namespace mm
