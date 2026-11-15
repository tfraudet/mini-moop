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

juce::String themeId (Theme t) { return t == Theme::classic ? "classic" : "modern"; }

Theme themeFromId (const juce::String& id) { return id == "classic" ? Theme::classic : Theme::modern; }

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
    const float scaleR = knobSize * (stepped ? 0.42f : 0.40f);
    const float labelR = knobSize * (stepped ? 0.535f : 0.48f);
    const float legendH = knobSize * (stepped ? 0.10f : 0.11f); // scale legend font height

    // ---- Scale legends
    g.setColour (colours::legend);
    auto pointAt = [&] (float angle, float radius) {
        return centre.getPointOnCircumference (radius, angle);
    };

    if (kind == "custom")
    {
        // Calibrated scale: ticks at given positions, legends on the labelled ones.
        for (const auto& token : juce::StringArray::fromTokens (s.getProperties()[props::labels].toString(), "|", ""))
        {
            const float frac = token.upToFirstOccurrenceOf ("=", false, false).getFloatValue();
            const auto txt = token.fromFirstOccurrenceOf ("=", false, false);
            const float a = startAngle + (endAngle - startAngle) * frac;
            const bool major = txt.isNotEmpty();
            g.drawLine (juce::Line<float> (pointAt (a, knobR + 2.5f), pointAt (a, major ? scaleR + 1.5f : scaleR - 1.5f)),
                        major ? 1.5f : 1.1f);
            if (major)
            {
                g.setFont (legendFont (legendH));
                const auto where = pointAt (a, labelR);
                const float legendW = txt.length() > 3 ? knobSize * 0.5f : knobSize * 0.26f;
                g.drawFittedText (txt, juce::Rectangle<float> (legendW, legendH * 1.2f).withCentre (where).toNearestInt(),
                                  juce::Justification::centred, 1);
            }
        }
    }
    else if (stepped)
    {
        const auto labels = juce::StringArray::fromTokens (s.getProperties()[props::labels].toString(), "|", "");
        const int n = labels.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * float (i) / float (std::max (1, n - 1));
            g.drawLine (juce::Line<float> (pointAt (a, knobR + 2.5f), pointAt (a, scaleR + 1.5f)), 1.8f);
            const auto c = pointAt (a, labelR);
            const auto box = juce::Rectangle<float> (knobSize * 0.23f, knobSize * 0.115f).withCentre (c);
            if (labels[i].startsWith ("~"))
                drawWaveIcon (g, box.reduced (box.getWidth() * 0.1f, box.getHeight() * 0.08f), labels[i]);
            else
            {
                g.setFont (legendFont (legendH));
                g.drawFittedText (labels[i], box.toNearestInt().expanded (6, 0), juce::Justification::centred, 1);
            }
        }
    }
    else
    {
        // Pot scale: 0..10 numbered every 2; bipolar pots numbered at both ends and
        // centre, or, with more than three legends, one legend every other tick
        // (TUNE: -2 -1 0 1 2 with a tick between each).
        const bool bip = kind == "bipolar";
        const auto labels = juce::StringArray::fromTokens (s.getProperties()[props::labels].toString(), "|", "");
        const bool spread = bip && labels.size() > 3;
        const int ticks = spread ? 2 * (labels.size() - 1) : 10;
        for (int i = 0; i <= ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * float (i) / float (ticks);
            const bool major = spread ? (i % 2 == 0) : bip ? (i % 5 == 0) : (i % 2 == 0);
            const float inner = knobR + 2.5f, outer = major ? scaleR + 1.5f : scaleR - 1.5f;
            g.drawLine (juce::Line<float> (pointAt (a, inner), pointAt (a, outer)), major ? 1.5f : 1.1f);
            const bool numbered = spread ? (i % 2 == 0) : bip ? (i == 0 || i == 5 || i == 10) : (i % 2 == 0);
            if (numbered)
            {
                juce::String txt;
                if (spread)
                    txt = labels[i / 2];
                else if (bip)
                    txt = labels.size() == 3 ? labels[i / 5] : juce::String (i == 0 ? "-" : i == 5 ? "0" : "+");
                else
                    txt = juce::String (i);
                g.setFont (legendFont (legendH));
                const auto c = pointAt (a, labelR);
                g.drawFittedText (txt, juce::Rectangle<float> (knobSize * 0.26f, legendH * 1.2f).withCentre (c).toNearestInt(),
                                  juce::Justification::centred, 1);
            }
        }
    }

    // ---- Knob body
    const float angle = startAngle + pos * (endAngle - startAngle);
    if (theme == Theme::classic && stepped)
        drawSelectorClassic (g, centre, knobR, angle, knobSize, s.isEnabled());
    else if (theme == Theme::classic)
        drawPotClassic (g, centre, knobR, angle, knobSize, s.isEnabled());
    else
        drawKnobModern (g, centre, knobR, angle, knobSize, s.isEnabled());
}

void PanelLookAndFeel::drawKnobModern (juce::Graphics& g, juce::Point<float> centre, float knobR, float angle,
                                       float knobSize, bool enabled)
{
    auto pointAt = [&] (float a, float radius) { return centre.getPointOnCircumference (radius, a); };

    // shadow, knurled skirt, cap, pointer
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

    if (! enabled)
    {
        g.setColour (colours::panel.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));
    }
}

void PanelLookAndFeel::drawTurnedCap (juce::Graphics& g, juce::Point<float> centre, float capR)
{
    // turned-aluminium cap: base shading, concentric tool marks, conical sheen
    using MC = juce::MathConstants<float>;
    auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };
    const float lightAngle = -MC::pi * 0.25f;
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.fillEllipse (circle (capR + 1.0f));
    juce::ColourGradient cap (juce::Colour (0xffeeeeea), centre.translated (-capR * 0.6f, -capR * 0.8f),
                              juce::Colour (0xff85857f), centre.translated (capR * 0.6f, capR * 0.9f), false);
    g.setGradientFill (cap);
    g.fillEllipse (circle (capR));
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<float> (circle (capR)).toNearestInt().expanded (1));
        const float step = std::max (0.7f, capR / 16.0f);
        int k = 0;
        for (float rr = step; rr < capR - 0.3f; rr += step, ++k)
        {
            g.setColour (k % 2 == 0 ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.07f));
            g.drawEllipse (circle (rr), 0.5f);
        }
        // conical sheen of the turned surface: light wedges facing and opposite
        // the light, darker ones across, each a stack of widening soft wedges
        for (auto [wedge, colour] : { std::pair { lightAngle, juce::Colours::white.withAlpha (0.09f) },
                                      std::pair { lightAngle + MC::pi, juce::Colours::white.withAlpha (0.05f) },
                                      std::pair { lightAngle + MC::halfPi, juce::Colours::black.withAlpha (0.04f) },
                                      std::pair { lightAngle - MC::halfPi, juce::Colours::black.withAlpha (0.04f) } })
            for (float span : { 0.15f, 0.3f, 0.45f, 0.6f })
            {
                juce::Path p;
                p.addPieSegment (circle (capR), wedge - span, wedge + span, 0.0f);
                g.setColour (colour);
                g.fillPath (p);
            }
    }
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (circle (capR), 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (circle (capR - 0.8f), 0.5f);
}

void PanelLookAndFeel::drawSelectorClassic (juce::Graphics& g, juce::Point<float> centre, float knobR, float angle,
                                            float knobSize, bool enabled)
{
    // Seen from above: a round black skirt under a bar grip that runs round the
    // turned-aluminium cap, narrow toward the pointer (with a white line on
    // it), flared with a hollowed end on the other side; both ends reach a
    // little past the skirt. Light from the upper left.
    auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };
    const float R = knobR, capR = R * 0.44f;

    // drop shadow and skirt
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (circle (R * 1.03f).translated (1.5f, 3.0f));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (circle (R * 1.1f).translated (2.5f, 4.5f));
    juce::ColourGradient skirt (juce::Colour (0xff2f2f31), centre.translated (-R * 0.75f, -R * 0.75f),
                                juce::Colour (0xff040404), centre.translated (R * 0.55f, R * 0.7f), false);
    skirt.addColour (0.45, juce::Colour (0xff121213));
    g.setGradientFill (skirt);
    g.fillEllipse (circle (R));
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawEllipse (circle (R - 0.6f), 0.9f);

    // Bar outline, built pointing up (-y) in units of R, then turned to `angle`.
    // Sides of the body follow a circle of radius 0.56 round the cap.
    constexpr float bodyR = 0.56f, tipHalf = 0.07f, tipY = -1.05f;
    constexpr float tailHalf = 0.42f, tailY = 1.06f, tailHollow = 0.9f;
    const juce::Point<float> shoulder { 0.2f, -std::sqrt (bodyR * bodyR - 0.04f) };  // tip flank meets the body
    const juce::Point<float> hip { 0.3f, std::sqrt (bodyR * bodyR - 0.09f) };        // tail flank leaves the body
    juce::Path bar;
    auto arcPoints = [&] (float fromA, float toA) {
        constexpr int steps = 16;
        for (int i = 1; i <= steps; ++i)
        {
            const float a = fromA + (toA - fromA) * float (i) / float (steps);
            bar.lineTo (bodyR * std::sin (a), -bodyR * std::cos (a));
        }
    };
    const float shoulderA = std::atan2 (shoulder.x, -shoulder.y), hipA = std::atan2 (hip.x, -hip.y);
    bar.startNewSubPath (-tipHalf, tipY);
    bar.quadraticTo (0.0f, tipY - 0.03f, tipHalf, tipY);   // slightly rounded tip
    bar.lineTo (shoulder.x, shoulder.y);
    arcPoints (shoulderA, hipA);                            // right side of the body
    bar.lineTo (tailHalf, tailY);
    bar.quadraticTo (0.0f, tailHollow, -tailHalf, tailY);   // hollowed tail end
    bar.lineTo (-hip.x, hip.y);
    arcPoints (-hipA, -shoulderA);                          // left side of the body
    bar.closeSubPath();
    bar.applyTransform (juce::AffineTransform::scale (R).rotated (angle).translated (centre));

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (bar, juce::AffineTransform::translation (1.0f, 2.2f));
    juce::ColourGradient barFill (juce::Colour (0xff343437), centre.translated (-R * 0.8f, -R * 0.8f),
                                  juce::Colour (0xff050505), centre.translated (R * 0.6f, R * 0.8f), false);
    barFill.addColour (0.4, juce::Colour (0xff151516));
    g.setGradientFill (barFill);
    g.fillPath (bar);
    // its top edge catches the light on the upper left
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.45f), centre.translated (-R * 0.7f, -R * 0.7f),
                              juce::Colours::white.withAlpha (0.2f), centre.translated (R * 0.7f, R * 0.7f), false);
    g.setGradientFill (rim);
    g.strokePath (bar, juce::PathStrokeType (std::max (0.8f, knobSize * 0.01f)));

    // pointer line on the narrow end
    g.setColour (juce::Colour (0xfff2f2ee));
    g.drawLine (juce::Line<float> (centre.getPointOnCircumference (capR + R * 0.18f, angle),
                                   centre.getPointOnCircumference (R * 0.98f, angle)),
                std::max (1.6f, knobSize * 0.024f));

    // cap seated in a dark ring
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (circle (capR + 2.0f));
    drawTurnedCap (g, centre, capR);

    if (! enabled)
    {
        g.setColour (colours::panel.withAlpha (0.5f));
        g.fillEllipse (circle (R));
    }
}

void PanelLookAndFeel::drawPotClassic (juce::Graphics& g, juce::Point<float> centre, float knobR, float angle,
                                       float knobSize, bool enabled)
{
    // Seen from above: a round black skirt, the scalloped edge of the grip's
    // top face close to it, a white index dot on that face and the
    // turned-aluminium cap. Light from the upper left.
    using MC = juce::MathConstants<float>;
    auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };
    const float lightAngle = -MC::pi * 0.25f;
    const float R = knobR, capR = R * 0.58f;
    constexpr int lobes = 8;

    // Grip outline: rounded lobes meeting in sharp flutes, turning with the knob
    // (a lobe faces the index dot).
    auto scallop = [&] (float outer, float inner) {
        juce::Path p;
        constexpr int steps = 360;
        for (int i = 0; i <= steps; ++i)
        {
            const float th = MC::twoPi * float (i) / float (steps);
            const float f = std::pow (std::abs (std::cos (0.5f * float (lobes) * (th - angle))), 0.6f);
            const auto pt = centre.getPointOnCircumference (inner + (outer - inner) * f, th);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    };

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (circle (R * 1.03f).translated (1.5f, 3.0f));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (circle (R * 1.1f).translated (2.5f, 4.5f));

    // skirt: plain black disc, slight sheen and a lit rim
    juce::ColourGradient skirt (juce::Colour (0xff2f2f31), centre.translated (-R * 0.75f, -R * 0.75f),
                                juce::Colour (0xff040404), centre.translated (R * 0.55f, R * 0.7f), false);
    skirt.addColour (0.45, juce::Colour (0xff121213));
    g.setGradientFill (skirt);
    g.fillEllipse (circle (R));
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawEllipse (circle (R - 0.6f), 0.9f);

    // grip: shadow on the skirt, black body, lobes shaded by the light they face
    const float gripOuter = R * 0.92f, gripInner = R * 0.84f;
    const auto grip = scallop (gripOuter, gripInner);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillPath (grip, juce::AffineTransform::translation (0.8f, 1.8f));
    juce::ColourGradient body (juce::Colour (0xff262628), centre.translated (-gripOuter, -gripOuter),
                               juce::Colour (0xff050505), centre.translated (gripOuter * 0.7f, gripOuter), false);
    g.setGradientFill (body);
    g.fillPath (grip);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (grip);
        for (int k = 0; k < lobes; ++k)
        {
            const float mid = angle + MC::twoPi * float (k) / float (lobes);
            const float half = MC::pi / float (lobes);
            const float facing = std::cos (mid - lightAngle);
            juce::Path lobe;
            lobe.addPieSegment (circle (gripOuter + 2.0f), mid - half, mid + half, (capR + 2.0f) / (gripOuter + 2.0f));
            g.setColour (facing > 0.0f ? juce::Colours::white.withAlpha (0.07f * facing)
                                       : juce::Colours::black.withAlpha (-0.2f * facing));
            g.fillPath (lobe);
        }
    }
    // the top face's wavy edge catches the light on the upper left
    const float edge = std::max (0.8f, knobSize * 0.01f);
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.38f), centre.translated (-gripOuter * 0.7f, -gripOuter * 0.7f),
                              juce::Colours::white.withAlpha (0.12f), centre.translated (gripOuter * 0.7f, gripOuter * 0.7f), false);
    g.setGradientFill (rim);
    g.strokePath (grip, juce::PathStrokeType (edge));

    // cap seated in a dark ring
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (circle (capR + 2.0f));
    drawTurnedCap (g, centre, capR);

    // index dot on the top face, in front of a lobe
    const float dotR = std::max (1.4f, R * 0.06f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (centre.getPointOnCircumference ((capR + gripInner) * 0.5f + 1.0f, angle)));

    if (! enabled)
    {
        g.setColour (colours::panel.withAlpha (0.5f));
        g.fillEllipse (circle (R));
    }
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const juce::String colourName = b.getProperties()[props::rocker].toString();
    const bool classic = theme == Theme::classic;
    const juce::Colour body = colourName == "orange" ? (classic ? colours::classicOrange : colours::rockerOrange)
                            : colourName == "white" ? colours::rockerWhite
                                                    : (classic ? colours::classicBlue : colours::rockerBlue);
    const bool horizontal = b.getProperties()[props::rockerHorizontal];
    const bool inactive = b.getProperties()[props::rockerInactive];
    const auto r = b.getLocalBounds().toFloat();
    if (classic)
        drawRockerClassic (g, r, body, horizontal, b.getToggleState(), highlighted, inactive);
    else
        drawRockerModern (g, r, body, horizontal, b.getToggleState(), highlighted, inactive);
}

void PanelLookAndFeel::drawRockerModern (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour body, bool horizontal,
                                         bool on, bool highlighted, bool inactive)
{
    const auto lit = body.brighter (highlighted && ! inactive ? 0.25f : 0.1f);
    const auto shade = body.darker (0.7f);

    if (horizontal)
    {
        // Rocker: horizontal paddle in a black bezel, pivoting on a vertical
        // ridge; the lit half shows the selected side (right half lit = ON).
        auto bezel = r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), r.getWidth() * 0.55f)).reduced (1.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (bezel, 3.0f);
        auto paddle = bezel.reduced (2.5f);
        auto left = paddle.removeFromLeft (paddle.getWidth() * 0.5f);
        auto right = paddle;
        g.setColour (on ? shade : lit);
        g.fillRoundedRectangle (left, 2.0f);
        g.setColour (on ? lit : shade);
        g.fillRoundedRectangle (right, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (bezel.getCentreX(), bezel.getY() + 3.0f, bezel.getCentreX(), bezel.getBottom() - 3.0f, 1.0f);
        if (inactive)
        {
            g.setColour (colours::panel.withAlpha (0.6f));
            g.fillRoundedRectangle (bezel, 3.0f);
        }
        return;
    }

    // Rocker: vertical paddle in a black bezel; the lit half shows the selected side (bottom half lit = ON).
    auto bezel = r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight() * 0.55f), r.getHeight()).reduced (1.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bezel, 3.0f);
    auto paddle = bezel.reduced (2.5f);
    auto top = paddle.removeFromTop (paddle.getHeight() * 0.5f);
    auto bottom = paddle;
    g.setColour (on ? shade : lit);
    g.fillRoundedRectangle (top, 2.0f);
    g.setColour (on ? lit : shade);
    g.fillRoundedRectangle (bottom, 2.0f);
    // ridge
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (bezel.getX() + 3.0f, bezel.getCentreY(), bezel.getRight() - 3.0f, bezel.getCentreY(), 1.0f);
    if (inactive)
    {
        g.setColour (colours::panel.withAlpha (0.6f));
        g.fillRoundedRectangle (bezel, 3.0f);
    }
}

void PanelLookAndFeel::drawRockerClassic (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour body, bool horizontal,
                                          bool on, bool highlighted, bool inactive)
{
    // Matte paddle in a black frame, rocking on its middle. The selected half
    // (right = ON, or bottom = ON) is pressed down: flat and even. The other
    // half stands up: the lip of its end catches the light from the upper
    // left, and its face darkens as it slopes down toward the pivot.
    auto bezel = horizontal ? r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), r.getWidth() * 0.55f)).reduced (1.0f)
                            : r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight() * 0.55f), r.getHeight()).reduced (1.0f);
    const auto base = body.brighter (highlighted && ! inactive ? 0.1f : 0.0f);

    // frame: outer shadow, black body, light line along its top edge
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bezel.translated (0.6f, 1.2f).expanded (0.5f), 3.0f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (bezel, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (bezel.getX() + 2.5f, bezel.getY() + 0.6f, bezel.getRight() - 2.5f, bezel.getY() + 0.6f, 0.8f);

    const float frame = std::max (1.8f, std::min (bezel.getWidth(), bezel.getHeight()) * 0.08f);
    const auto paddle = bezel.reduced (frame);
    const float corner = std::min (paddle.getWidth(), paddle.getHeight()) * 0.12f;
    juce::Path outline;
    outline.addRoundedRectangle (paddle, corner);

    // Points along the paddle, from the raised end (t = 0) to the pressed end (t = 1).
    auto at = [&] (float t) {
        const float along = on ? t : 1.0f - t;
        return horizontal ? juce::Point<float> (paddle.getX() + paddle.getWidth() * along, paddle.getCentreY())
                          : juce::Point<float> (paddle.getCentreX(), paddle.getY() + paddle.getHeight() * along);
    };
    // part of the paddle between t0 and t1
    auto span = [&] (float t0, float t1) {
        const auto p0 = at (t0), p1 = at (t1);
        return horizontal ? juce::Rectangle<float> (std::min (p0.x, p1.x), paddle.getY(), std::abs (p1.x - p0.x), paddle.getHeight())
                          : juce::Rectangle<float> (paddle.getX(), std::min (p0.y, p1.y), paddle.getWidth(), std::abs (p1.y - p0.y));
    };

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (outline);

        juce::ColourGradient faces (base.brighter (0.06f), at (0.0f), base.darker (0.04f), at (1.0f), false);
        faces.addColour (0.05, base.darker (0.02f));    // just behind the lip
        faces.addColour (0.25, base.darker (0.22f));
        faces.addColour (0.42, base.darker (0.4f));     // bottom of the slope, by the pivot
        faces.addColour (0.53, base);                   // pressed half: flat
        g.setGradientFill (faces);
        g.fillRect (paddle);

        // the frame's shadow along the top (or left) edge of the paddle
        const auto edgeBand = horizontal ? paddle.withHeight (paddle.getHeight() * 0.18f) : paddle.withWidth (paddle.getWidth() * 0.18f);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.18f), edgeBand.getTopLeft(),
                                    juce::Colours::transparentBlack, horizontal ? edgeBand.getBottomLeft() : edgeBand.getTopRight(), false);
        g.setGradientFill (inner);
        g.fillRect (edgeBand);

        // lip of the raised end: a bright rim along its end and corners, fading
        // along the paddle's edges
        {
            juce::Graphics::ScopedSaveState lipClip (g);
            const auto lip = span (0.0f, 0.22f);
            g.reduceClipRegion (lip.toNearestInt().expanded (1));
            juce::ColourGradient rim (base.brighter (0.5f).withAlpha (0.6f), at (0.0f), base.withAlpha (0.0f), at (0.22f), false);
            g.setGradientFill (rim);
            g.strokePath (outline, juce::PathStrokeType (std::max (1.2f, corner * 0.6f)));
            juce::ColourGradient glow (juce::Colours::white.withAlpha (0.06f), at (0.0f), juce::Colours::transparentWhite, at (0.1f), false);
            g.setGradientFill (glow);
            g.fillRect (lip);
        }
    }

    if (inactive)
    {
        g.setColour (colours::panel.withAlpha (0.6f));
        g.fillRoundedRectangle (bezel, 2.5f);
    }
}

} // namespace mm::ui
