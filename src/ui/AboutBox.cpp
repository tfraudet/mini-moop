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

#include "AboutBox.h"

#include "Logo.h"
#include "PanelLookAndFeel.h"

#ifndef MINIMOOP_VERSION_STRING
 #define MINIMOOP_VERSION_STRING "1.0.0"
#endif
#ifndef MINIMOOP_BUILD_DATE
 #define MINIMOOP_BUILD_DATE __DATE__
#endif

namespace mm::ui
{

AboutBox::AboutBox (const juce::Image& woodTexture) : wood (woodTexture)
{
    setWantsKeyboardFocus (true);
    setInterceptsMouseClicks (true, true);
    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);
}

void AboutBox::resized()
{
    const auto card = cardBounds().toNearestInt();
    closeButton.setBounds (card.getRight() - 14 - 30, card.getY() + 14, 30, 30);
}

void AboutBox::show()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void AboutBox::hide() { setVisible (false); }

void AboutBox::mouseDown (const juce::MouseEvent&) { hide(); }

bool AboutBox::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey)
    {
        hide();
        return true;
    }
    return false;
}

juce::Rectangle<float> AboutBox::cardBounds() const
{
    return juce::Rectangle<float> (840.0f, 380.0f).withCentre (getLocalBounds().toFloat().getCentre());
}

void AboutBox::paint (juce::Graphics& g)
{
    // dim the instrument behind
    g.fillAll (juce::Colours::black.withAlpha (0.65f));

    const auto card = cardBounds();
    juce::Path shape;
    shape.addRoundedRectangle (card, 10.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 24, { 0, 8 }).drawForPath (g, shape);
    juce::ColourGradient face (juce::Colour (0xff26262a), card.getTopLeft(), juce::Colour (0xff161618), card.getBottomLeft(), false);
    g.setGradientFill (face);
    g.fillPath (shape);
    g.setColour (colours::lcdText.withAlpha (0.55f));
    g.strokePath (shape, juce::PathStrokeType (1.2f));

    auto inner = card.reduced (22.0f, 20.0f);
    auto left = inner.removeFromLeft (200.0f);
    inner.removeFromLeft (28.0f);

    const auto logoArea = left.removeFromTop (180.0f).withSizeKeepingCentre (170.0f, 170.0f);
    drawLogo (g, logoArea, wood);

    // Name, version, tagline
    g.setColour (colours::legend);
    g.setFont (PanelLookAndFeel::legendFont (42.0f));
    g.drawText ("mini moop", inner.removeFromTop (50.0f), juce::Justification::centredLeft);
    g.setColour (colours::lcdText);
    g.setFont (PanelLookAndFeel::lcdFont (19.0f));
    g.drawText ("Version " MINIMOOP_VERSION_STRING, inner.removeFromTop (26.0f), juce::Justification::centredLeft);
    inner.removeFromTop (8.0f);
    g.setColour (colours::legend);
    g.setFont (PanelLookAndFeel::legendFont (16.5f));
    g.drawText ("Component-level simulation of the Minimoog Model D", inner.removeFromTop (24.0f), juce::Justification::centredLeft);
    inner.removeFromTop (8.0f);

    // Description
    const juce::String description =
        "Instead of imitating the sound, Mini Moop recreates the circuits of the classic 1970s "
        "monophonic synthesizer: transistors, diodes, capacitors and op-amps are calculated thousands "
        "of times per second. Each voice is a slightly different, individually calibrated unit that "
        "drifts with temperature like the original. Available as VST3, AU and standalone, with an "
        "optional polyphonic mode.";
    g.setColour (colours::legendDim);
    g.setFont (PanelLookAndFeel::legendFont (15.0f, false));
    g.drawFittedText (description, inner.removeFromTop (86.0f).toNearestInt(), juce::Justification::topLeft, 5, 1.0f);
    inner.removeFromTop (8.0f);

    // Facts
    auto fact = [&] (const juce::String& key, const juce::String& value) {
        auto row = inner.removeFromTop (24.0f);
        g.setColour (colours::stripLabel);
        g.setFont (PanelLookAndFeel::legendFont (13.0f));
        g.drawText (key, row.removeFromLeft (92.0f), juce::Justification::centredLeft);
        g.setColour (colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (15.0f, false));
        g.drawText (value, row, juce::Justification::centredLeft);
    };
    fact ("AUTHOR", "Thierry Fraudet");
    fact ("DATE", MINIMOOP_BUILD_DATE);
    fact ("LICENSE", "GNU Affero General Public License v3.0 or later");
    fact ("FORMATS", juce::String::fromUTF8 ("VST3  \xc2\xb7  AU  \xc2\xb7  Standalone"));

    // Footer under the logo
    g.setColour (colours::legendDim.withAlpha (0.8f));
    g.setFont (PanelLookAndFeel::legendFont (11.5f, false));
    left.removeFromTop (18.0f);
    g.drawFittedText ("Built with JUCE (AGPLv3).\nMinimoog and Model D are trademarks of Moog Music Inc. "
                      "Mini Moop is an independent project, not affiliated with Moog Music.",
                      left.withX (logoArea.getX()).withWidth (logoArea.getWidth()).toNearestInt(),
                      juce::Justification::centredTop, 8, 1.0f);

    g.setColour (colours::legendDim.withAlpha (0.6f));
    g.setFont (PanelLookAndFeel::legendFont (11.5f, false));
    g.drawText ("Click anywhere or press Esc to close", card.withTop (card.getBottom() - 24.0f), juce::Justification::centred);
}

void CloseButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, {}, highlighted, down);
    const auto r = getLocalBounds().toFloat();
    const float half = r.getHeight() * 0.2f, thick = std::max (1.5f, r.getHeight() * 0.075f);
    const auto c = r.getCentre();
    juce::Path cross;
    cross.startNewSubPath (c.x - half, c.y - half);
    cross.lineTo (c.x + half, c.y + half);
    cross.startNewSubPath (c.x + half, c.y - half);
    cross.lineTo (c.x - half, c.y + half);
    g.setColour (highlighted ? colours::lcdText : colours::legend);
    g.strokePath (cross, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BurgerButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, {}, highlighted, down);
    const auto r = getLocalBounds().toFloat();
    const float w = r.getWidth() * 0.46f, gap = r.getHeight() * 0.17f, thick = std::max (1.5f, r.getHeight() * 0.075f);
    g.setColour (highlighted ? colours::lcdText : colours::legend);
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (juce::Rectangle<float> (w, thick).withCentre (r.getCentre().translated (0.0f, float (i) * gap)), thick * 0.5f);
}

} // namespace mm::ui
