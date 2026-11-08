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
// Thermal model of a voice's boards:
//   * board temperature: slow Ornstein-Uhlenbeck wander (minutes)
//   * per-site local temperature (air currents over components, seconds)
//   * per-expo-pair temperature difference between the two transistors of the
//     "matched" pair (CA3046 array: same die, so very small)
// The expo converters read these temperatures; the resulting pitch / cutoff
// drift is a consequence of the device physics, not an added random LFO.

#include "../circuit/components/Passives.h"

#include <array>
#include <cmath>

namespace mm
{

class OrnsteinUhlenbeck
{
public:
    void prepare (double updateRate, double tauSeconds, double sigma) noexcept
    {
        a = std::exp (-1.0 / (updateRate * tauSeconds));
        b = sigma * std::sqrt (1.0 - a * a);
    }
    double next (Rng& rng) noexcept { x = a * x + b * rng.normal(); return x; }
    double value() const noexcept { return x; }
    void set (double v) noexcept { x = v; }

private:
    double a = 0.0, b = 0.0, x = 0.0;
};

class DriftModel
{
public:
    enum Site { Vco1 = 0, Vco2, Vco3, Tempco1, Tempco2, Tempco3, FilterPair, FilterTempco, numSites };

    void prepare (double controlRate, uint64_t seed) noexcept
    {
        rng.reseed (seed);
        board.prepare (controlRate, 60.0, 0.6);
        for (auto& s : sites) s.prepare (controlRate, 4.0, 0.08);
        for (auto& d : pairDelta) d.prepare (controlRate, 1.5, 0.004);
        board.set (0.6 * rng.normal());
        for (auto& s : sites) s.set (0.08 * rng.normal());
        for (auto& d : pairDelta) d.set (0.004 * rng.normal());
    }

    /** 0 = lab-stable, 1 = typical vintage instrument, 3 = on a hot stage. */
    void setAmount (double amt) noexcept { amount = amt; }

    void tick() noexcept
    {
        board.next (rng);
        for (auto& s : sites) s.next (rng);
        for (auto& d : pairDelta) d.next (rng);
    }

    double temperature (int site) const noexcept
    {
        return nominalK + amount * (board.value() + sites[size_t (site)].value());
    }

    /** Temperature difference between the output and reference transistor of expo pair `i` (0..3). */
    double pairDifference (int i) const noexcept { return amount * pairDelta[size_t (i)].value(); }

    double nominalK = 298.15;

private:
    Rng rng;
    OrnsteinUhlenbeck board;
    std::array<OrnsteinUhlenbeck, numSites> sites;
    std::array<OrnsteinUhlenbeck, 4> pairDelta;
    double amount = 1.0;
};

} // namespace mm
