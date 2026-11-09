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

#include "PanelLookAndFeel.h"
#include "LedDisplay.h"

namespace mm::ui
{

PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Label::textColourId, colours::legend);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111113));
    setColour (juce::ComboBox::textColourId, colours::legend);
    setColour (juce::ComboBox::outlineColourId, colours::legendDim.withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId, colours::legend);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff151517));
    setColour (juce::PopupMenu::textColourId, colours::legend);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::rockerOrange.withAlpha (0.6f));
    setColour (juce::Slider::textBoxTextColourId, colours::lcdText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::lcdBack);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff111113));
    setColour (juce::TextButton::textColourOffId, colours::legend);
}

juce::Font PanelLookAndFeel::legendFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Helvetica Neue", height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font PanelLookAndFeel::lcdFont (float height)
{
    return juce::Font (juce::FontOptions ("Menlo", height, juce::Font::bold));
}

void PanelLookAndFeel::drawLcdWindow (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    const auto inner = r.reduced (1.5f);
    juce::ColourGradient glass (colours::lcdBack.brighter (0.08f), inner.getTopLeft(),
                                colours::lcdBack, inner.getBottomLeft(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (inner, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (int (r.getBottom()) - 1, r.getX() + 2.0f, r.getRight() - 2.0f);
}

juce::Font PanelLookAndFeel::getLabelFont (juce::Label& l) { return legendFont (l.getFont().getHeight()); }

void PanelLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (! l.getProperties()["mmLed"] || l.isBeingEdited())
    {
        LookAndFeel_V4::drawLabel (g, l);
        return;
    }
    const auto r = l.getLocalBounds().toFloat();
    led::drawWindow (g, r);
    led::drawText (g, r.reduced (3.0f, 5.0f), l.getText(), colours::lcdText, juce::Justification::centred);
    led::drawGlassSheen (g, r);
}
juce::Font PanelLookAndFeel::getComboBoxFont (juce::ComboBox&) { return lcdFont (15.0f); }

void PanelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, float (width), float (height));
    led::drawWindow (g, r);
    led::drawText (g, r.reduced (4.0f, 5.0f).withTrimmedRight (16.0f), box.getText(),
                   colours::lcdText.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f), juce::Justification::centredLeft,
                   juce::Time::getMillisecondCounter());
    led::drawGlassSheen (g, r);
    // small amber triangle
    juce::Path arrow;
    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour (colours::lcdText.withAlpha (box.isEnabled() ? 0.9f : 0.4f));
    g.fillPath (arrow);
}

void PanelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    // The text is drawn as LED segments by drawComboBox; the label stays invisible.
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
}

juce::Label* PanelLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->getProperties().set ("mmLed", true); // drawn as an LED display by drawLabel
    l->setFont (lcdFont (15.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, colours::lcdText);
    l->setColour (juce::Label::backgroundColourId, colours::lcdBack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::black);
    l->setColour (juce::TextEditor::textColourId, colours::lcdText);
    l->setColour (juce::TextEditor::backgroundColourId, colours::lcdBack);
    return l;
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    juce::ColourGradient face (juce::Colour (0xff3a3a3d).brighter (highlighted ? 0.15f : 0.0f), r.getTopLeft(),
                               juce::Colour (0xff222224), r.getBottomLeft(), false);
    if (down) face.multiplyOpacity (0.7f);
    g.setGradientFill (face);
    g.fillRoundedRectangle (r.reduced (1.0f), 2.5f);
}

void PanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (colours::legend.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.setFont (legendFont (14.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0.0f, 0.0f, style, s);
        return;
    }
    const float cy = float (y) + float (h) * 0.5f;
    // slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (float (x), cy - 2.0f, float (w), 4.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (int (cy + 2.0f), float (x), float (x + w));
    // orange fader cap
    auto cap = juce::Rectangle<float> (12.0f, float (h) * 0.8f).withCentre ({ pos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (1.0f, 1.5f), 2.0f);
    juce::ColourGradient body (colours::rockerOrange.brighter (0.25f), cap.getTopLeft(),
                               colours::rockerOrange.darker (0.5f), cap.getBottomLeft(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawHorizontalLine (int (cap.getCentreY()), cap.getX() + 2.0f, cap.getRight() - 2.0f);
}
juce::Font PanelLookAndFeel::getPopupMenuFont() { return legendFont (16.0f, false); }

void PanelLookAndFeel::drawWaveIcon (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
{
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom();
    const float xm = r.getCentreX(), w = r.getWidth();
    if (t == "~tri")
    {
        p.startNewSubPath (x0, y1); p.lineTo (x0 + w * 0.25f, y0); p.lineTo (xm, y1);
        p.lineTo (x0 + w * 0.75f, y0); p.lineTo (x1, y1);
    }
    else if (t == "~shark")
    {
        p.startNewSubPath (x0, y1); p.lineTo (x0 + w * 0.35f, y0); p.lineTo (xm, y1);
        p.lineTo (xm + w * 0.35f, y0); p.lineTo (x1, y1);
    }
    else if (t == "~rsaw")
    {
        p.startNewSubPath (x0, y0); p.lineTo (xm, y1); p.lineTo (xm, y0); p.lineTo (x1, y1);
    }
    else if (t == "~saw")
    {
        p.startNewSubPath (x0, y1); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, y0);
    }
    else
    {
        const float duty = t == "~sq" ? 0.5f : t == "~wide" ? 0.3f : 0.15f;
        const float half = w * 0.5f;
        p.startNewSubPath (x0, y1);
        for (int k = 0; k < 2; ++k)
        {
            const float s = x0 + float (k) * half;
            p.lineTo (s, y0); p.lineTo (s + half * duty, y0); p.lineTo (s + half * duty, y1); p.lineTo (s + half, y1);
        }
    }
    g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::mitered));
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (w), float (h));
    // Knob geometry follows its nominal diameter; the component box may be larger
    // (selector switches need room for their legends outside the scale).
    const juce::String kind = s.getProperties()[props::scale].toString();
    const float boxSize = std::min (bounds.getWidth(), bounds.getHeight());
    const float knobSize = float (s.getProperties().getWithDefault (props::knobSize, boxSize));
    const auto centre = bounds.getCentre();
    const bool stepped = kind == "stepped";
    const float knobR = knobSize * 0.33f;
    const float scaleR = knobSize * (stepped ? 0.42f : 0.39f);
    const float labelR = knobSize * (stepped ? 0.53f : 0.45f);

    // ---- Scale legends
    g.setColour (colours::legend);
    auto pointAt = [&] (float angle, float radius) {
        return centre.getPointOnCircumference (radius, angle);
    };

    if (stepped)
    {
        const auto labels = juce::StringArray::fromTokens (s.getProperties()[props::labels].toString(), "|", "");
        const int n = labels.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * float (i) / float (std::max (1, n - 1));
            g.drawLine (juce::Line<float> (pointAt (a, knobR + 2.5f), pointAt (a, scaleR)), 1.5f);
            const auto c = pointAt (a, labelR);
            const auto box = juce::Rectangle<float> (knobSize * 0.2f, knobSize * 0.1f).withCentre (c);
            if (labels[i].startsWith ("~"))
                drawWaveIcon (g, box.reduced (box.getWidth() * 0.12f, box.getHeight() * 0.1f), labels[i]);
            else
            {
                g.setFont (legendFont (knobSize * 0.085f));
                g.drawFittedText (labels[i], box.toNearestInt().expanded (6, 0), juce::Justification::centred, 1);
            }
        }
    }
    else
    {
        // Pot scale: 0..10 numbered every 2; bipolar pots numbered at both ends and centre
        const bool bip = kind == "bipolar";
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * float (i) / 10.0f;
            const bool major = bip ? (i % 5 == 0) : (i % 2 == 0);
            const float inner = knobR + 2.5f, outer = major ? scaleR + 1.0f : scaleR - 2.0f;
            g.drawLine (juce::Line<float> (pointAt (a, inner), pointAt (a, outer)), 1.0f);
            const bool numbered = bip ? (i == 0 || i == 5 || i == 10) : (i % 2 == 0);
            if (numbered)
            {
                juce::String txt;
                if (bip)
                {
                    const auto labels = juce::StringArray::fromTokens (s.getProperties()[props::labels].toString(), "|", "");
                    txt = labels.size() == 3 ? labels[i / 5] : juce::String (i == 0 ? "-" : i == 5 ? "0" : "+");
                }
                else
                    txt = juce::String (i);
                g.setFont (legendFont (knobSize * 0.085f));
                const auto c = pointAt (a, labelR);
                g.drawFittedText (txt, juce::Rectangle<float> (knobSize * 0.18f, knobSize * 0.1f).withCentre (c).toNearestInt(),
                                  juce::Justification::centred, 1);
            }
        }
    }

    // ---- Knob: shadow, knurled skirt, cap, pointer
    const float angle = startAngle + pos * (endAngle - startAngle);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.15f, knobR * 2.15f).withCentre (centre.translated (1.5f, 2.5f)));

    juce::ColourGradient skirt (juce::Colour (0xff2c2c2e), centre.translated (-knobR, -knobR),
                                colours::knobSkirt, centre.translated (knobR, knobR), false);
    g.setGradientFill (skirt);
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

    // knurling
    g.setColour (juce::Colour (0xff3a3a3d));
    const int teeth = 36;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * float (i) / float (teeth);
        g.drawLine (juce::Line<float> (pointAt (a, knobR * 0.86f), pointAt (a, knobR * 0.99f)), 0.8f);
    }

    // metal cap
    const float capR = knobR * 0.62f;
    juce::ColourGradient cap (juce::Colour (0xffe2e2dc), centre.translated (-capR * 0.6f, -capR * 0.8f),
                              juce::Colour (0xff6d6d6a), centre.translated (capR * 0.6f, capR * 0.9f), false);
    g.setGradientFill (cap);
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre), 0.8f);

    // pointer line on the skirt and cap
    g.setColour (juce::Colours::white);
    g.drawLine (juce::Line<float> (pointAt (angle, capR * 0.25f), pointAt (angle, knobR * 0.97f)), std::max (1.6f, knobSize * 0.022f));

    if (! s.isEnabled())
    {
        g.setColour (colours::panel.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));
    }
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const juce::String colourName = b.getProperties()[props::rocker].toString();
    const juce::Colour body = colourName == "orange" ? colours::rockerOrange
                            : colourName == "white" ? colours::rockerWhite : colours::rockerBlue;

    // Rocker: vertical paddle in a black bezel; the pressed half is darker.
    auto bezel = r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight() * 0.55f), r.getHeight()).reduced (1.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bezel, 3.0f);
    auto paddle = bezel.reduced (2.5f);
    const bool on = b.getToggleState();
    auto top = paddle.removeFromTop (paddle.getHeight() * 0.5f);
    auto bottom = paddle;
    const auto lit = body.brighter (highlighted ? 0.25f : 0.1f);
    const auto shade = body.darker (0.7f);
    g.setColour (on ? lit : shade);
    g.fillRoundedRectangle (top, 2.0f);
    g.setColour (on ? shade : lit);
    g.fillRoundedRectangle (bottom, 2.0f);
    // ridge
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (bezel.getX() + 3.0f, bezel.getCentreY(), bezel.getRight() - 3.0f, bezel.getCentreY(), 1.0f);
}

} // namespace mm::ui
