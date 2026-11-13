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
// Front-panel state as seen by the circuits: pot positions (0..1, i.e. the
// 0..10 panel scale / 10) and switch positions. Plus the few non-original
// settings (poly, quality, drift...).

#include "../circuit/blocks/VcoCore.h"

#include <array>

namespace mm
{

enum class Range { Lo = 0, Ft32, Ft16, Ft8, Ft4, Ft2 };

struct OscPanel
{
    Range range = Range::Ft8;
    Waveform wave = Waveform::Sawtooth;
    double frequency = 0.5;   // osc 2 / 3 detune pot, 0.5 = centre
    double volume = 1.0;
    bool on = true;
};

struct PanelState
{
    // Controllers
    double tune = 0.5;              // master tune pot, 0.5 = centre
    double glide = 0.0;
    double modMix = 0.0;            // 0 = osc 3, 1 = noise
    bool oscModulation = false;
    bool filterModulation = false;
    bool osc3Control = true;        // osc 3 follows keyboard

    // Oscillator bank
    std::array<OscPanel, 3> osc {};

    // Mixer
    double extVolume = 0.0;
    bool extOn = false;
    double noiseVolume = 0.0;
    bool noiseOn = false;
    bool noisePink = false;

    // Filter
    double cutoff = 0.6;
    double emphasis = 0.0;
    double contourAmount = 0.5;
    bool kbd1 = false;              // 1/3 keyboard control
    bool kbd2 = false;              // 2/3 keyboard control
    double fAttack = 0.0, fDecay = 0.4, fSustain = 0.6;

    // Loudness contour
    double lAttack = 0.0, lDecay = 0.4, lSustain = 1.0;

    // Output
    double volume = 0.7;
    bool mainOn = true;
    bool a440 = false;

    // Left-hand controller
    bool glideOn = false;
    bool decayOn = false;
    double pitchWheel = 0.0;        // -1..1
    double modWheel = 0.0;          // 0..1

    // Non-original
    bool feedbackPatch = false;     // output patched into EXTERNAL INPUT
    double bendRange = 7.0;         // semitones each way: Fig. 9-2's "+/-5TH", as the plug-in's default
    double drift = 1.0;             // 0..3
    double modAmount = 0.0;         // 0..1, stored modulation depth (acts as a minimum MOD. wheel position)

    /** Moves the continuous controls a fraction `a` towards `t` and copies
        the switches: a real pot cannot jump, so neither do ours. */
    void smoothToward (const PanelState& t, double a) noexcept
    {
        auto s = [a] (double& v, double target) { v += a * (target - v); };
        PanelState old = *this;
        *this = t;
        auto keep = [] (double& v, double o) { v = o; };
        keep (tune, old.tune); s (tune, t.tune);
        keep (glide, old.glide); s (glide, t.glide);
        keep (modMix, old.modMix); s (modMix, t.modMix);
        for (size_t i = 0; i < osc.size(); ++i)
        {
            keep (osc[i].frequency, old.osc[i].frequency); s (osc[i].frequency, t.osc[i].frequency);
            keep (osc[i].volume, old.osc[i].volume); s (osc[i].volume, t.osc[i].volume);
        }
        keep (extVolume, old.extVolume); s (extVolume, t.extVolume);
        keep (noiseVolume, old.noiseVolume); s (noiseVolume, t.noiseVolume);
        keep (cutoff, old.cutoff); s (cutoff, t.cutoff);
        keep (emphasis, old.emphasis); s (emphasis, t.emphasis);
        keep (contourAmount, old.contourAmount); s (contourAmount, t.contourAmount);
        keep (volume, old.volume); s (volume, t.volume);
        keep (pitchWheel, old.pitchWheel); s (pitchWheel, t.pitchWheel);
        keep (modWheel, old.modWheel); s (modWheel, t.modWheel);
        keep (modAmount, old.modAmount); s (modAmount, t.modAmount);
    }
};

} // namespace mm
