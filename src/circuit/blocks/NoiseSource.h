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
// tilts it towards -3 dB/octave (pink).
//
// The junction noise is Gaussian white with a weak 1/f excess (flicker) and
// occasional "popcorn" steps, filtered by the amplifier's own bandwidth.
// The pink network is board 3's (Fig. 9-8, circuit 3): R16 10K from the white
// noise follower Q4 into two series RC branches to -10 V (C3 .12 uF with R8
// 3.3K, C2 with R13) and, through C1 2.5 uF, the base of the amplifier Q3
// (its input resistance, about 92K with R7/R6's bootstrap, and its gain,
// 14.6, fitted to ngspice: within 0.2 dB from 30 Hz to 20 kHz). C2 and R13 are
// the CA-72 project's hardware reference's .022 uF and 510 (drawn .033 uF and
// 220): its pink against its white then matches within 0.45 dB from 125 Hz to
// 12.7 kHz. The third-order network is integrated with the bilinear transform.
//
// Red noise (board 3, circuit 3, pin 2): the pink noise through R12 10k into
// C7 0.15 uF (one pole at 106 Hz), buffered by the emitter follower Q6. It
// only feeds the modulation bus (see Voice: the WHITE / PINK switch selects
// white or pink for the mixer, and pink or red for the MODULATION MIX).

#include "../components/Passives.h"

#include <array>
#include <cmath>

namespace mm
{

/** The modulation path keeps the level its calibration gave the pink noise
    before board 3's network replaced the generic one (whose pink was 19 dB
    quieter): the noise reaching MODULATION MIX is unchanged in RMS. */
inline constexpr double pinkToModulationScale = 0.0943 / 0.7778; // pink RMS before / after (192 kHz)

class NoiseSource
{
public:
    /** Gain from the pink / red noise outputs to the MODULATION MIX input.
        Oscillator 3 and the noise enter MODULATION MIX through equal 24k
        resistors (Fig. 9-8, circuit 15), so the noise is meant to be of the
        same order as oscillator 3. The noise outputs' levels are not
        documented: this gain puts the pink noise at about 0.4 times
        oscillator 3's peak there (0.68 V rms against 1.7 V); the red noise
        keeps its physical level relative to the pink (about half). */
    static constexpr double modulationGain = 11.0 * (1.7 / 2.5) * pinkToModulationScale;

    void prepare (double sampleRate, uint64_t seed) noexcept
    {
        fs = sampleRate;
        rng.reseed (seed);
        // Amplifier bandwidth ~ 40 kHz (one pole)
        ampCoeff = 1.0 - std::exp (-2.0 * M_PI * std::fmin (40000.0, 0.45 * fs) / fs);
        // Flicker component: leaky integrator corner ~ 20 Hz
        flickerCoeff = std::exp (-2.0 * M_PI * 20.0 / fs);
        makePinkNetwork();
        pinkState.fill (0.0);
        // Red noise: R12 10k / C7 0.15 uF low-pass (one pole)
        redCoeff = 1.0 - std::exp (-2.0 * M_PI * (1.0 / (2.0 * M_PI * 10.0e3 * 0.15e-6)) / fs);
        // Output coupling capacitors (e.g. C28 10 uF into R59 100k: 0.16 Hz):
        // no DC reaches the mixer or the modulation bus, so the rare popcorn
        // steps and the slowest flicker die away instead of offsetting pitch.
        const double rc = 10.0e-6 * 100.0e3;
        dcCoeff = rc / (rc + 1.0 / fs);
    }

    void setWhiteLevel (double voltsRms) noexcept { level = voltsRms; }

    /** Produces one sample of white, pink and red noise (volts). */
    void process (double& white, double& pink, double& red) noexcept
    {
        const double g = rng.normal();
        flicker = flickerCoeff * flicker + (1.0 - flickerCoeff) * rng.normal() * 6.0;
        if (rng.uniform() < 2.0e-6) popcorn = popcorn == 0.0 ? 0.03 : 0.0; // rare burst noise (small: it would dominate the pink / red outputs)
        const double raw = g + 0.15 * flicker + popcorn;
        amp += ampCoeff * (raw - amp);
        white = level * amp;

        // Pink network, direct form II transposed
        const double pinkRaw = pinkB[0] * white + pinkState[0];
        pinkState[0] = pinkB[1] * white - pinkA[1] * pinkRaw + pinkState[1];
        pinkState[1] = pinkB[2] * white - pinkA[2] * pinkRaw + pinkState[2];
        pinkState[2] = pinkB[3] * white - pinkA[3] * pinkRaw;
        redState += redCoeff * (pinkRaw - redState);
        white = couple (dcWhite, white);
        pink = couple (dcPink, pinkRaw);
        red = couple (dcRed, redState);
    }

private:
    struct Coupling { double x1 = 0.0, y1 = 0.0; };

    /** One-pole high-pass of an output coupling capacitor. */
    double couple (Coupling& c, double x) const noexcept
    {
        const double y = dcCoeff * (c.y1 + x - c.x1);
        c.x1 = x;
        c.y1 = y;
        return y;
    }

    /** Board 3's pink network as H(s) = A / (1 + R16 Y(s)), Y the admittance
        of the two RC branches and of C1 into Q3's input, by the bilinear
        transform. */
    void makePinkNetwork() noexcept
    {
        constexpr double r16 = 10.0e3, r8 = 3.3e3, c3 = 0.12e-6, r13 = 510.0, c2 = 0.022e-6;
        constexpr double c1 = 2.5e-6, rIn = 92.0e3, gain = 14.6;
        using P = std::array<double, 4>; // ascending powers of s
        auto mul1 = [] (const P& p, double t) { P r {}; for (int i = 0; i < 4; ++i) { r[size_t (i)] += p[size_t (i)]; if (i < 3) r[size_t (i + 1)] += t * p[size_t (i)]; } return r; };
        const double t1 = r8 * c3, t2 = r13 * c2, tb = rIn * c1;
        const P one { 1, 0, 0, 0 };
        const P d = mul1 (mul1 (mul1 (one, t1), t2), tb);
        // s C (1 + s ta)(1 + s tb') for each branch
        auto branch = [&] (double c, double ta, double tb2) { P q = mul1 (mul1 (one, ta), tb2); P r {}; for (int i = 0; i < 3; ++i) r[size_t (i + 1)] = c * q[size_t (i)]; return r; };
        const P y3 = branch (c3, t2, tb), y2 = branch (c2, t1, tb), yb = branch (c1, t1, t2);
        P num {}, den {};
        for (size_t i = 0; i < 4; ++i)
        {
            num[i] = gain * d[i];
            den[i] = d[i] + r16 * (y3[i] + y2[i] + yb[i]);
        }
        // Bilinear: s = K (1 - z^-1) / (1 + z^-1); multiply through by (1 + z^-1)^3.
        const double k = 2.0 * fs;
        auto toZ = [k] (const P& p) {
            P z {};
            for (int i = 0; i < 4; ++i)
            {
                // (1 - z^-1)^i (1 + z^-1)^(3 - i)
                P t { 1, 0, 0, 0 };
                for (int a = 0; a < i; ++a) { P n {}; for (int j = 0; j < 4; ++j) { n[size_t (j)] += t[size_t (j)]; if (j < 3) n[size_t (j + 1)] -= t[size_t (j)]; } t = n; }
                for (int a = i; a < 3; ++a) { P n {}; for (int j = 0; j < 4; ++j) { n[size_t (j)] += t[size_t (j)]; if (j < 3) n[size_t (j + 1)] += t[size_t (j)]; } t = n; }
                const double ki = std::pow (k, double (i));
                for (int j = 0; j < 4; ++j) z[size_t (j)] += p[size_t (i)] * ki * t[size_t (j)];
            }
            return z;
        };
        const P bz = toZ (num), az = toZ (den);
        for (size_t i = 0; i < 4; ++i)
        {
            pinkB[i] = bz[i] / az[0];
            pinkA[i] = az[i] / az[0];
        }
    }

    Rng rng;
    std::array<double, 4> pinkB {}, pinkA {};
    std::array<double, 3> pinkState {};
    double fs = 48000.0, ampCoeff = 1.0, flickerCoeff = 0.99;
    double amp = 0.0, flicker = 0.0, popcorn = 0.0, level = 1.0;
    double redCoeff = 0.0, redState = 0.0, dcCoeff = 1.0;
    Coupling dcWhite, dcPink, dcRed;
};

} // namespace mm
