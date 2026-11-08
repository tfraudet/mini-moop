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
// Behavioural macro-model of a 1970s general-purpose op-amp (741 / 301 class):
// single-pole open-loop response (finite gain-bandwidth), slew-rate limit and
// soft saturation a little below the supply rails.

#include <algorithm>
#include <cmath>

namespace mm
{

struct OpAmpParams
{
    double gbw        = 1.0e6;   // Hz
    double slewRate   = 0.5e6;   // V/s  (741: 0.5 V/us)
    double railPos    = 10.0;     // the instrument runs on regulated +/-10 V
    double railNeg    = -10.0;
    double headroom   = 1.5;     // output swings to rail - headroom
    double offsetV    = 0.0;     // input offset voltage (tolerance draw)

    static OpAmpParams ua741 (double rail = 10.0) noexcept { return { 1.0e6, 0.5e6, rail, -rail, 1.5, 0.0 }; }
    static OpAmpParams lm301 (double rail = 10.0) noexcept { return { 1.0e6, 10.0e6, rail, -rail, 1.2, 0.0 }; }
};

/** Op-amp in a closed-loop configuration with ideal-gain `closedGain` and a
    feedback factor beta. The closed loop bandwidth is GBW*beta; the output
    is slew-limited and soft-clipped near the rails. */
class OpAmpStage
{
public:
    void prepare (const OpAmpParams& params, double sampleRate, double noiseGain) noexcept
    {
        p = params;
        fs = sampleRate;
        const double fc = std::min (p.gbw / std::max (1.0, noiseGain), 0.45 * fs);
        coeff = 1.0 - std::exp (-2.0 * M_PI * fc / fs);
        maxStep = p.slewRate / fs;
        vMax = p.railPos - p.headroom;
        vMin = p.railNeg + p.headroom;
    }

    void reset (double v = 0.0) noexcept { y = v; }

    /** `target` = ideal closed-loop output (incl. offset * noise gain). */
    double process (double target) noexcept
    {
        target = softClip (target);
        const double delta = std::clamp (coeff * (target - y), -maxStep, maxStep);
        y += delta;
        return y;
    }

    double softClip (double v) const noexcept
    {
        // Output stage saturation: hard-ish knee over ~0.5 V.
        const double knee = 0.5;
        if (v > vMax - knee)
            return vMax - knee + knee * std::tanh ((v - (vMax - knee)) / knee);
        if (v < vMin + knee)
            return vMin + knee + knee * std::tanh ((v - (vMin + knee)) / knee);
        return v;
    }

    const OpAmpParams& params() const noexcept { return p; }
    double output() const noexcept { return y; }

private:
    OpAmpParams p;
    double fs = 48000.0, coeff = 1.0, maxStep = 1.0, vMax = 13.5, vMin = -13.5;
    double y = 0.0;
};

} // namespace mm
