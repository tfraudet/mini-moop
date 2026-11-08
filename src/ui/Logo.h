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
// The Mini Moop logo, drawn as vectors: a rounded-square metal tile with a
// zigzag "M" routed into it and inlaid with wood.

#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::ui
{

inline void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, const juce::Image& wood)
{
    const float s = std::min (area.getWidth(), area.getHeight());
    const auto r = juce::Rectangle<float> (s, s).withCentre (area.getCentre());

    // Tile
    juce::Path tile;
    tile.addRoundedRectangle (r.reduced (s * 0.03f), s * 0.22f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::jmax (2, int (s * 0.06f)), { 0, juce::jmax (1, int (s * 0.03f)) })
        .drawForPath (g, tile);
    juce::ColourGradient metal (juce::Colour (0xffbfc4cc), r.getTopLeft(), juce::Colour (0xff666c75), r.getBottomLeft(), false);
    metal.addColour (0.5, juce::Colour (0xff959ba4));
    g.setGradientFill (metal);
    g.fillPath (tile);
    // rim: light upper edge, darker lower edge
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tile);
        juce::Path inner;
        inner.addRectangle (r.expanded (s * 0.1f));
        inner.setUsingNonZeroWinding (false);
        inner.addRoundedRectangle (r.reduced (s * 0.03f).translated (0.0f, s * 0.012f), s * 0.22f);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillPath (inner);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (tile, juce::PathStrokeType (juce::jmax (0.6f, s * 0.006f)));

    // Zigzag "M"
    auto pt = [&] (float u, float v) { return juce::Point<float> (r.getX() + u * s, r.getY() + v * s); };
    juce::Path line;
    line.startNewSubPath (pt (0.19f, 0.662f));
    line.lineTo (pt (0.372f, 0.345f));
    line.lineTo (pt (0.50f, 0.561f));
    line.lineTo (pt (0.628f, 0.345f));
    line.lineTo (pt (0.81f, 0.662f));
    juce::Path groove;
    juce::PathStrokeType (s * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (groove, line);

    // light catching the lower lip of the groove
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillPath (groove, juce::AffineTransform::translation (0.0f, s * 0.006f));

    // wood inlay
    if (wood.isValid())
    {
        const float scale = s * 1.4f / float (wood.getWidth());
        g.setFillType (juce::FillType (wood, juce::AffineTransform::scale (scale)
                                                 .rotated (juce::MathConstants<float>::halfPi * 0.85f, r.getCentreX(), r.getCentreY())
                                                 .translated (r.getX(), r.getY())));
    }
    else
        g.setColour (juce::Colour (0xff8a5630));
    g.fillPath (groove);
    juce::ColourGradient warm (juce::Colour (0x30ff9a40), r.getTopLeft(), juce::Colour (0x40000000), r.getBottomLeft(), false);
    g.setGradientFill (warm);
    g.fillPath (groove);

    // shadow cast on the wood by the upper wall of the groove
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (groove);
    juce::Path outside;
    outside.addRectangle (r.expanded (s * 0.1f));
    outside.setUsingNonZeroWinding (false);
    outside.addPath (groove, juce::AffineTransform::translation (0.0f, s * 0.014f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (outside);
}

} // namespace mm::ui
