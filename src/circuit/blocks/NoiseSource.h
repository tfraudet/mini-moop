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
// Noise generator (board 3, circuit 3, Fig. 9-8; service manual 2.5): the
// avalanche noise of Q15's reverse-biased base-emitter junction, amplified by
// Q12 (NOISE LEVEL R26 in its emitter) and buffered by Q4: the white noise.
//
// The junction noise is Gaussian white with a weak 1/f excess (flicker) and
// occasional "popcorn" steps, filtered by the amplifier's own bandwidth. That
// bandwidth is not on the drawing: it is the one for which the three outputs
// come out closest to one level, as R. A. Moog drawing 1431 marks them ("-4
// dBm" on white, pink and red): 37 kHz, pink then 1.1 dB above white and red
// 0.7 dB below (tools/spice/noise_levels.py).
//
// Pink: R16 10K from Q4 into two series RC branches (C3 .12 uF with R8 3.3K,
// C2 .033 uF with R13 220) and, through C1 2.5 uF, the base of the amplifier
// Q3. The board's lower rail is -10 V (drawing 1431: Q3's collector at
// +3.8 V), which sets Q3 at about 2.3 mA: its input resistance (27K) and its
// gain (R3 3.3K beside R12 and the loads, over R1 120: 16.9) are ngspice's on
// the drawn circuit (tools/spice/noise.cir). The third-order network is
// integrated with the bilinear transform.
//
// Red noise: the pink noise through R12 10K into C7 0.15 uF (106 Hz), then the
// common emitter stage Q6: R24 10K load beside the output's load, R15 3.9K
// emitter, gain 1.81 (ngspice), C22 0.15 uF across the load (a second pole,
// 149 Hz). It only feeds the modulation bus: the WHITE / PINK switch selects
// white or pink for the mixer, and pink or red for MODULATION MIX.
//
// The outputs are volts at the board's pins. The white noise's level is the
// NOISE LEVEL trim: Voice sets it as the service manual does, for -5 dB at the
// main output.

#include "../components/Passives.h"

#include <array>
#include <cmath>

namespace mm
{

class NoiseSource
{
public:
    /** The white noise's bandwidth (Hz): equal levels on the three outputs (drawing 1431). */
    static constexpr double whiteBandwidth = 37000.0;
    /** Q6's gain from C8 to its collector: the load over R15 3.9K and its own re (ngspice). */
    static constexpr double redGain = 1.81;

    void prepare (double sampleRate, uint64_t seed) noexcept
    {
        fs = sampleRate;
        rng.reseed (seed);
        // Amplifier bandwidth (one pole): see above. The generator's samples
        // are scaled so that the white noise has the same density at every
        // sample rate: that of `level` volts rms spread over the noise
        // bandwidth (pi / 2 times the pole) of the full 37 kHz, also where
        // the pole itself has to sit lower, below Nyquist. What lies above
        // Nyquist is then lost, not folded into the audio band.
        const double fc = std::fmin (whiteBandwidth, 0.45 * fs);
        ampCoeff = 1.0 - std::exp (-2.0 * M_PI * fc / fs);
        density = std::sqrt (0.5 * fs / (0.5 * M_PI * whiteBandwidth));
        // Flicker component: leaky integrator corner ~ 20 Hz
        flickerCoeff = std::exp (-2.0 * M_PI * 20.0 / fs);
        makePinkNetwork();
        pinkState.fill (0.0);
        // Red noise: R12 10K into C7 0.15 uF, then Q6's load (R24 10K beside
        // R28 2.2M and the output's 25K) against C22 0.15 uF.
        auto lowPass = [this] (double rc) { return 1.0 - std::exp (-1.0 / (rc * fs)); };
        constexpr double rRedLoad = 1.0 / (1.0 / 10.0e3 + 1.0 / 2.2e6 + 1.0 / 25.0e3);
        red1Coeff = lowPass (10.0e3 * 0.15e-6);
        red2Coeff = lowPass (rRedLoad * 0.15e-6);
        // Output coupling capacitors: no DC reaches the mixer or the modulation
        // bus, so the rare popcorn steps and the slowest flicker die away
        // instead of offsetting pitch. Pink C20 2.5 uF and red C28 10 uF into
        // about 25K (2.5 and 0.6 Hz); white C18 .15 uF into the NOISE VOLUME
        // pot's 25K (42 Hz).
        auto coupling = [this] (double c, double r) { const double rc = c * r; return rc / (rc + 1.0 / fs); };
        dcCoeff = coupling (2.5e-6, 25.0e3);
        dcRedCoeff = coupling (10.0e-6, 25.0e3);
        dcWhiteCoeff = coupling (0.15e-6, 25.0e3);
    }

    /** The white noise's level (V rms in the amplifier's bandwidth): the NOISE LEVEL trim. */
    void setWhiteLevel (double voltsRms) noexcept { level = voltsRms; }

    /** Produces one sample of white, pink and red noise (volts). */
    void process (double& white, double& pink, double& red) noexcept
    {
        const double g = rng.normal();
        flicker = flickerCoeff * flicker + (1.0 - flickerCoeff) * rng.normal() * 6.0;
        if (rng.uniform() < 2.0e-6) popcorn = popcorn == 0.0 ? 0.03 : 0.0; // rare burst noise (small: it would dominate the pink / red outputs)
        const double raw = g + 0.15 * flicker + popcorn;
        amp += ampCoeff * (raw - amp);
        white = level * density * amp;

        // Pink network, direct form II transposed
        const double pinkRaw = pinkB[0] * white + pinkState[0];
        pinkState[0] = pinkB[1] * white - pinkA[1] * pinkRaw + pinkState[1];
        pinkState[1] = pinkB[2] * white - pinkA[2] * pinkRaw + pinkState[2];
        pinkState[2] = pinkB[3] * white - pinkA[3] * pinkRaw;
        red1 += red1Coeff * (pinkRaw - red1);
        red2 += red2Coeff * (redGain * red1 - red2);
        white = couple (dcWhite, white, dcWhiteCoeff);
        pink = couple (dcPink, pinkRaw, dcCoeff);
        red = couple (dcRed, red2, dcRedCoeff);
    }

private:
    struct Coupling { double x1 = 0.0, y1 = 0.0; };

    /** One-pole high-pass of an output coupling capacitor. */
    static double couple (Coupling& c, double x, double coeff) noexcept
    {
        const double y = coeff * (c.y1 + x - c.x1);
        c.x1 = x;
        c.y1 = y;
        return y;
    }

    /** Board 3's pink network as H(s) = A / (1 + R16 Y(s)), Y the admittance
        of the two RC branches and of C1 into Q3's input, by the bilinear
        transform. */
    void makePinkNetwork() noexcept
    {
        constexpr double r16 = 10.0e3, r8 = 3.3e3, c3 = 0.12e-6, r13 = 220.0, c2 = 0.033e-6;
        constexpr double c1 = 2.5e-6, rIn = 26.9e3, gain = 16.9; // Q3 (tools/spice/noise.cir)
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
    double fs = 48000.0, ampCoeff = 1.0, flickerCoeff = 0.99, density = 1.0;
    double amp = 0.0, flicker = 0.0, popcorn = 0.0, level = 1.0;
    double red1Coeff = 0.0, red2Coeff = 0.0, red1 = 0.0, red2 = 0.0;
    double dcCoeff = 1.0, dcRedCoeff = 1.0, dcWhiteCoeff = 1.0;
    Coupling dcWhite, dcPink, dcRed;
};

} // namespace mm
