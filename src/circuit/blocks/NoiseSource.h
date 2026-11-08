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
// Noise source: shot/avalanche noise of a reverse-biased transistor junction,
// amplified by a transistor stage (white), plus the passive RC network that
// tilts it by -3 dB/octave (pink).
//
// The junction noise is Gaussian white with a weak 1/f excess (flicker) and
// occasional "popcorn" steps, filtered by the amplifier's own bandwidth.
// The pink network is the classic three-section RC ladder (three
// real poles / zeros interleaved), integrated with the bilinear transform.

#include "../components/Passives.h"

#include <array>
#include <cmath>

namespace mm
{

class NoiseSource
{
public:
    void prepare (double sampleRate, uint64_t seed) noexcept
    {
        fs = sampleRate;
        rng.reseed (seed);
        // Amplifier bandwidth ~ 40 kHz (one pole)
        ampCoeff = 1.0 - std::exp (-2.0 * M_PI * std::fmin (40000.0, 0.45 * fs) / fs);
        // Flicker component: leaky integrator corner ~ 20 Hz
        flickerCoeff = std::exp (-2.0 * M_PI * 20.0 / fs);
        // Pink network: poles/zeros spaced for an average -3 dB/oct slope
        const double poles[3] = { 40.0, 400.0, 4000.0 };
        const double zeros[3] = { 120.0, 1200.0, 12000.0 };
        for (int i = 0; i < 3; ++i)
            sections[size_t (i)] = makeShelf (poles[i], zeros[i]);
        // Pink network loses level; restore 0 dB at ~1 kHz equivalent loudness.
        pinkGain = 2.2;
    }

    void setWhiteLevel (double voltsRms) noexcept { level = voltsRms; }

    /** Produces one sample of white and pink noise (volts). */
    void process (double& white, double& pink) noexcept
    {
        const double g = rng.normal();
        flicker = flickerCoeff * flicker + (1.0 - flickerCoeff) * rng.normal() * 6.0;
        if (rng.uniform() < 2.0e-6) popcorn = popcorn == 0.0 ? 0.15 : 0.0; // rare burst noise
        const double raw = g + 0.15 * flicker + popcorn;
        amp += ampCoeff * (raw - amp);
        white = level * amp;

        double x = white;
        for (auto& s : sections)
        {
            const double y = s.b0 * x + s.b1 * s.x1 - s.a1 * s.y1;
            s.x1 = x;
            s.y1 = y;
            x = y;
        }
        pink = pinkGain * x;
    }

private:
    struct Section { double b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0; };

    /** First-order RC shelf: pole at fp, zero at fz (fz > fp), unity DC gain,
        bilinear transform with frequency prewarping. */
    Section makeShelf (double fp, double fz) const noexcept
    {
        const double wp = std::tan (M_PI * fp / fs), wz = std::tan (M_PI * fz / fs);
        // H(s) = (1 + s/wz) / (1 + s/wp)
        const double a0 = 1.0 + 1.0 / wp;
        Section s;
        s.b0 = (1.0 + 1.0 / wz) / a0;
        s.b1 = (1.0 - 1.0 / wz) / a0;
        s.a1 = (1.0 - 1.0 / wp) / a0;
        return s;
    }

    Rng rng;
    std::array<Section, 3> sections;
    double fs = 48000.0, ampCoeff = 1.0, flickerCoeff = 0.99, pinkGain = 1.0;
    double amp = 0.0, flicker = 0.0, popcorn = 0.0, level = 1.0;
};

} // namespace mm
