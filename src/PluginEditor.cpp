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

#include "PluginEditor.h"

MiniMoopEditor::MiniMoopEditor (MiniMoopProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    constexpr int w = mm::ui::Panel::baseWidth, h = mm::ui::Panel::baseHeight;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (double (w) / double (h));
    setResizeLimits (w / 2, h / 2, w * 2, h * 2);
    setSize (int (w * 0.85), int (h * 0.85));
}

void MiniMoopEditor::resized()
{
    const float scale = float (getWidth()) / float (mm::ui::Panel::baseWidth);
    panel.setTransform (juce::AffineTransform::scale (scale));
}
