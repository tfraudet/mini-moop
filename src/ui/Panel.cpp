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

#include "Panel.h"

#include "../Parameters.h"
#include "../PluginProcessor.h"
#include "../Presets.h"
#include "LedDisplay.h"
#include "Tooltips.h"

#include "MiniMoopAssets.h"

namespace mm::ui
{

namespace id = mm::params::id;

// ============================================================================ Wheel

Wheel::Wheel (std::atomic<float>& target, bool bip, juce::String name)
    : value (target), bipolar (bip)
{
    setName (name);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

bool Wheel::refresh()
{
    const float v = value.load();
    if (std::abs (v - shown) < 1.0e-4f) return false;
    shown = v;
    return true;
}

void Wheel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    // slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 4.0f);
    auto wheel = r.reduced (4.0f, 6.0f);
    // wheel body with ridges that move with the value
    juce::ColourGradient body (juce::Colour (0xff2a2a2c), wheel.getTopLeft(), juce::Colour (0xff0e0e0f), wheel.getBottomLeft(), false);
    body.addColour (0.5, juce::Colour (0xff3a3a3d));
    g.setGradientFill (body);
    g.fillRoundedRectangle (wheel, 3.0f);
    const float norm = bipolar ? (shown + 1.0f) * 0.5f : shown;
    const float offset = (1.0f - norm) * wheel.getHeight() * 0.6f;
    g.setColour (juce::Colour (0xff4b4b4f));
    for (float y = wheel.getY() + std::fmod (offset, 7.0f); y < wheel.getBottom(); y += 7.0f)
        g.drawHorizontalLine (int (y), wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
    // position marker
    const float my = wheel.getY() + wheel.getHeight() * (1.0f - norm);
    g.setColour (colours::legend);
    g.fillRect (juce::Rectangle<float> (wheel.getX() + 2.0f, my - 1.5f, wheel.getWidth() - 4.0f, 3.0f));
}

void Wheel::mouseDown (const juce::MouseEvent&) { dragStartValue = value.load(); }

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    const float span = bipolar ? 2.0f : 1.0f;
    const float v = dragStartValue - span * float (e.getDistanceFromDragStartY()) / float (getHeight());
    value.store (juce::jlimit (bipolar ? -1.0f : 0.0f, 1.0f, v));
    if (refresh()) repaint();
}

void Wheel::mouseUp (const juce::MouseEvent&)
{
    if (bipolar) value.store (0.0f); // spring-loaded
    if (refresh()) repaint();
}

// ============================================================================ Keyboard

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour lineColour, juce::Colour textColour)
{
    auto key = area.reduced (0.5f, 0.0f);
    // A pressed key travels down: shorter visible face, darker tone.
    juce::ColourGradient face (juce::Colour (0xfff7f1e3), key.getTopLeft(),
                               juce::Colour (isDown ? 0xffd9cfb8 : 0xffe9e0cb), key.getBottomLeft(), false);
    face.addColour (0.85, juce::Colour (isDown ? 0xffe2d9c4 : 0xfff2ebdb));
    g.setGradientFill (face);
    g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f);
    if (isOver && ! isDown)
    {
        g.setColour (findColour (mouseOverKeyOverlayColourId));
        g.fillRect (key);
    }
    if (isDown)
    {
        g.setColour (findColour (keyDownOverlayColourId).withMultipliedAlpha (0.5f));
        g.fillRect (key);
    }
    // front lip of the key
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRect (key.withTop (key.getBottom() - (isDown ? 3.0f : 6.0f)));
    // separator
    g.setColour (lineColour);
    g.drawLine (area.getRight(), area.getY(), area.getRight(), area.getBottom(), 1.0f);

    const auto text = getWhiteNoteText (note);
    if (text.isNotEmpty())
    {
        g.setColour (textColour);
        g.setFont (PanelLookAndFeel::legendFont (9.0f, false));
        g.drawText (text, key.withTrimmedBottom (10.0f).withTop (key.getBottom() - 24.0f), juce::Justification::centred);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    // shadow on the white keys
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.translated (2.0f, 2.0f), 2.5f);

    juce::ColourGradient body (juce::Colour (0xff2b2b2d), area.getTopLeft(), juce::Colour (0xff050505), area.getBottomLeft(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (area, 2.5f);

    // sloped front of the key and glossy top
    const float front = isDown ? 4.0f : 9.0f;
    auto top = area.reduced (area.getWidth() * 0.12f, 0.0f).withTrimmedBottom (front);
    juce::ColourGradient gloss (juce::Colour (isDown ? 0xff1a1a1b : 0xff232325), top.getTopLeft(),
                                juce::Colour (0xff0b0b0c), top.getBottomLeft(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (top, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (isDown ? 0.05f : 0.10f));
    g.drawLine (top.getX() + 1.5f, top.getY(), top.getX() + 1.5f, top.getBottom() - 2.0f, 1.0f);

    if (isDown || isOver)
    {
        g.setColour (findColour (isDown ? keyDownOverlayColourId : mouseOverKeyOverlayColourId).withMultipliedAlpha (0.6f));
        g.fillRoundedRectangle (area, 2.5f);
    }
}

// ============================================================================ Panel

namespace
{
// Base-coordinate geometry (design size 1400 x 768)
constexpr float caseLeft = 34.0f, caseRight = 1366.0f;
const juce::Rectangle<float> frontPanel { caseLeft, 26.0f, caseRight - caseLeft, 420.0f };
const juce::Rectangle<float> settingsStrip { caseLeft, 460.0f, caseRight - caseLeft, 60.0f };
const juce::Rectangle<float> keyBed { caseLeft, 532.0f, caseRight - caseLeft, 216.0f };
constexpr float titleLine = 398.0f;
constexpr float leatherScale = 0.4f; // grain size: 1920 px tile -> 768 panel units

/** Turns a texture photo into a seamlessly tiling image:
    1. flattens the photo's large-scale lighting (subtracts a heavily blurred copy),
    2. cross-fades an overlap band of each edge into the opposite edge, so the
       tile's right/bottom border continues exactly into its left/top border. */
juce::Image makeSeamlessTile (const juce::Image& photo)
{
    const int w = photo.getWidth(), h = photo.getHeight();
    auto rgb = photo.convertedToFormat (juce::Image::RGB);

    // low-frequency lighting map: shrink to a few pixels, then stretch back smoothly
    juce::Image small (juce::Image::RGB, 24, 16, false), lighting (juce::Image::RGB, w, h, false);
    {
        juce::Graphics gs (small);
        gs.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        gs.drawImage (rgb, 0, 0, 24, 16, 0, 0, w, h);
        juce::Graphics gl (lighting);
        gl.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        gl.drawImage (small, 0, 0, w, h, 0, 0, 24, 16);
    }

    std::vector<float> flat (size_t (w * h * 3));
    {
        const juce::Image::BitmapData src (rgb, juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData low (lighting, juce::Image::BitmapData::readOnly);
        double mean[3] {};
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = src.getPixelColour (x, y), l = low.getPixelColour (x, y);
                float* f = &flat[size_t ((y * w + x) * 3)];
                f[0] = float (c.getRed() - l.getRed());
                f[1] = float (c.getGreen() - l.getGreen());
                f[2] = float (c.getBlue() - l.getBlue());
                mean[0] += l.getRed(); mean[1] += l.getGreen(); mean[2] += l.getBlue();
            }
        for (size_t i = 0; i < flat.size(); ++i)
            flat[i] += float (mean[i % 3] / double (w * h));
    }

    // Edge cross-fade. Tile size (w - ox) x (h - oy); for the first `o` pixels the
    // tile blends from the pixels just past its far edge (t = 0) to its own (t = 1),
    // so the wrap-around neighbour of the last pixel is its true neighbour in the photo.
    const int ox = w / 6, oy = h / 6;
    const int tw = w - ox, th = h - oy;
    auto at = [&] (int x, int y, int k) { return flat[size_t ((y * w + x) * 3 + k)]; };
    auto blendX = [&] (int x, int y, int k) {
        if (x >= ox) return at (x, y, k);
        const float t = (float (x) + 0.5f) / float (ox);
        return t * at (x, y, k) + (1.0f - t) * at (x + tw, y, k);
    };

    juce::Image tile (juce::Image::RGB, tw, th, false);
    juce::Image::BitmapData out (tile, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < th; ++y)
        for (int x = 0; x < tw; ++x)
        {
            float v[3];
            for (int k = 0; k < 3; ++k)
            {
                if (y >= oy)
                    v[k] = blendX (x, y, k);
                else
                {
                    const float t = (float (y) + 0.5f) / float (oy);
                    v[k] = t * blendX (x, y, k) + (1.0f - t) * blendX (x, y + th, k);
                }
            }
            auto c8 = [] (float f) { return (juce::uint8) juce::jlimit (0.0f, 255.0f, f); };
            out.setPixelColour (x, y, juce::Colour (c8 (v[0]), c8 (v[1]), c8 (v[2])));
        }
    return tile;
}
const juce::Range<float> sectionX[] = { { 34, 234 }, { 234, 614 }, { 614, 844 }, { 844, 1224 }, { 1224, 1366 } };
const char* sectionTitle[] = { "CONTROLLERS", "OSCILLATOR BANK", "MIXER", "MODIFIERS", "OUTPUT" };
} // namespace

Panel::Panel (MiniMoopProcessor& p)
    : proc (p),
      pitchWheel (p.pitchWheel, true, "PITCH"),
      modWheel (p.modWheel, false, "MOD."),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lnf);
    setSize (baseWidth, baseHeight);
    wood = juce::ImageCache::getFromMemory (MiniMoopAssets::wood_jpg, MiniMoopAssets::wood_jpgSize);
    brushed = juce::ImageCache::getFromMemory (MiniMoopAssets::brushed_jpg, MiniMoopAssets::brushed_jpgSize);
    {
        // Panel covering: the photo (with a stray speck cloned over) made into a
        // seamless tile, so it can be drawn at a fine grain size.
        const auto src = juce::ImageCache::getFromMemory (MiniMoopAssets::leather_jpg, MiniMoopAssets::leather_jpgSize);
        if (src.isValid())
        {
            leather = src.createCopy();
            juce::Graphics tg (leather);
            const float sx = float (src.getWidth()) / 1920.0f, sy = float (src.getHeight()) / 1282.0f;
            const int px = int (1062.0f * sx), py = int (732.0f * sy), pw = int (30.0f * sx), ph = int (30.0f * sy);
            tg.drawImage (src, px, py, pw, ph, px - int (70.0f * sx), py, pw, ph);
            leather = makeSeamlessTile (leather);
        }
    }

    // ------------------------------------------------------------ CONTROLLERS
    knob (id::tune, { 134, 86 }, 92, "bipolar", "-|0|+");
    knobLegend ("TUNE", { 134, 86 }, 92);
    knob (id::glide, { 86, 214 }, 92);
    knobLegend ("GLIDE", { 86, 214 }, 92);
    knob (id::modMix, { 182, 214 }, 92);
    knobLegend ("MOD. MIX", { 182, 214 }, 92);
    rocker (id::oscMod, { 82, 298, 24, 44 }, "orange");
    legend ("OSC.\nMOD.", { 64.0f, 346.0f, 60.0f, 24.0f }, 9.0f);
    rocker (id::modSource, { 162, 298, 24, 44 }, "orange");
    legend ("F. EG", { 149.0f, 285.0f, 50.0f, 11.0f }, 8.0f, juce::Justification::centred, true);
    legend ("NOISE", { 149.0f, 346.0f, 50.0f, 12.0f }, 9.0f);

    // -------------------------------------------------------- OSCILLATOR BANK
    const float colX[3] = { 330.0f, 440.0f, 552.0f };
    legend ("RANGE", { colX[0] - 50.0f, 40.0f, 100.0f, 13.0f }, 10.5f);
    legend ("FREQUENCY", { colX[1] - 50.0f, 40.0f, 100.0f, 13.0f }, 10.5f);
    legend ("WAVEFORM", { colX[2] - 50.0f, 40.0f, 100.0f, 13.0f }, 10.5f);
    const int rowY[3] = { 114, 224, 334 };
    for (int i = 0; i < 3; ++i)
    {
        const juce::String waveLabels = i == 2 ? "~tri|~rsaw|~saw|~sq|~wide|~narrow" : "~tri|~shark|~saw|~sq|~wide|~narrow";
        knob (id::oscRange[i], { int (colX[0]), rowY[i] }, 92, "stepped", "LO|32'|16'|8'|4'|2'", 116);
        knob (id::oscWave[i], { int (colX[2]), rowY[i] }, 92, "stepped", waveLabels, 116);
        if (i > 0)
        {
            knob (id::oscFreq[i], { int (colX[1]), rowY[i] }, 92, "bipolar", "-7|0|+7");
            knobLegend ("OSCILLATOR " + juce::String (i + 1), { int (colX[1]), rowY[i] }, 92);
        }
    }
    legend ("OSCILLATOR 1", { colX[1] - 55.0f, float (rowY[0]) - 7.0f, 110.0f, 14.0f }, 11.0f);
    rocker (id::osc3Control, { 244, 310, 24, 44 }, "orange");
    legend ("OSC. 3\nCTRL", { 226.0f, 357.0f, 60.0f, 24.0f }, 9.0f);

    // ------------------------------------------------------------------ MIXER
    legend ("VOLUME", { 630.0f, 40.0f, 80.0f, 13.0f }, 10.5f, juce::Justification::centredLeft);
    rocker (id::noiseColor, { 812, 32, 20, 34 }, "white");
    legend ("PINK", { 772.0f, 34.0f, 36.0f, 11.0f }, 8.0f, juce::Justification::centredRight, true);
    legend ("WHITE", { 772.0f, 53.0f, 36.0f, 11.0f }, 8.0f, juce::Justification::centredRight, true);
    const int mixY[5] = { 98, 163, 228, 293, 358 };
    const char* mixVol[5] = { id::oscVol[0], id::extVol, id::oscVol[1], id::noiseVol, id::oscVol[2] };
    const char* mixOn[5] = { id::oscOn[0], id::extOn, id::oscOn[1], id::noiseOn, id::oscOn[2] };
    const char* mixName[5] = { "OSC. 1", "EXT. INPUT", "OSC. 2", "NOISE", "OSC. 3" };
    for (int i = 0; i < 5; ++i)
    {
        knob (mixVol[i], { 660, mixY[i] }, 72);
        rocker (mixOn[i], { 708, mixY[i] - 19, 22, 38 }, "blue");
        legend (mixName[i], { 740.0f, float (mixY[i]) - 7.0f, 96.0f, 14.0f }, 10.0f, juce::Justification::centredLeft);
    }

    // -------------------------------------------------------------- MODIFIERS
    heading ("FILTER", 858.0f, 40.0f, 1212.0f);
    rocker (id::filterMod, { 874, 78, 24, 44 }, "orange");
    legend ("FILTER\nMOD.", { 856.0f, 126.0f, 60.0f, 24.0f }, 9.0f);
    const float modX[3] = { 966.0f, 1064.0f, 1162.0f };
    knob (id::cutoff, { int (modX[0]), 102 }, 92);
    knobLegend ("CUTOFF", { int (modX[0]), 102 }, 92);
    knob (id::emphasis, { int (modX[1]), 102 }, 92);
    knobLegend ("EMPHASIS", { int (modX[1]), 102 }, 92);
    knob (id::contourAmt, { int (modX[2]), 102 }, 92);
    knobLegend ("CONTOUR AMT.", { int (modX[2]), 102 }, 92);

    heading ("FILTER CONTOUR", 858.0f, 174.0f, 1212.0f);
    rocker (id::kbd1, { 862, 202, 24, 44 }, "orange");
    rocker (id::kbd2, { 890, 202, 24, 44 }, "orange");
    legend (juce::String::fromUTF8 ("KBD 1 \xc2\xb7 2"), { 851.0f, 250.0f, 74.0f, 12.0f }, 9.0f);
    heading ("LOUDNESS CONTOUR", 858.0f, 290.0f, 1212.0f);
    const char* fEnv[3] = { id::fAttack, id::fDecay, id::fSustain };
    const char* lEnv[3] = { id::lAttack, id::lDecay, id::lSustain };
    const char* envName[3] = { "ATTACK", "DECAY", "SUSTAIN" };
    for (int i = 0; i < 3; ++i)
    {
        knob (fEnv[i], { int (modX[i]), 228 }, 80);
        knobLegend (envName[i], { int (modX[i]), 228 }, 80);
        knob (lEnv[i], { int (modX[i]), 344 }, 80);
        knobLegend (envName[i], { int (modX[i]), 344 }, 80);
    }

    // ----------------------------------------------------------------- OUTPUT
    knob (id::volume, { 1295, 88 }, 92);
    knobLegend ("VOLUME", { 1295, 88 }, 92);
    rocker (id::mainOn, { 1283, 192, 24, 46 }, "blue");
    legend ("ON", { 1265.0f, 242.0f, 60.0f, 12.0f }, 9.5f);
    rocker (id::a440, { 1283, 296, 24, 46 }, "orange");
    legend ("A-440", { 1265.0f, 346.0f, 60.0f, 12.0f }, 9.5f);

    // ------------------------------------------------- left-hand controller
    rocker (id::glideOn, { 58, 538, 22, 38 }, "orange");
    legend ("GLIDE", { 44.0f, 578.0f, 50.0f, 11.0f }, 9.0f);
    rocker (id::decayOn, { 116, 538, 22, 38 }, "blue");
    legend ("DECAY", { 102.0f, 578.0f, 50.0f, 11.0f }, 9.0f);
    pitchWheel.setTooltip ("PITCH wheel: bends all oscillators (range set by BEND). Springs back to centre. Also driven by MIDI pitch bend.");
    modWheel.setTooltip ("MOD. wheel: amount of modulation sent through the OSC. MOD. / FILTER MOD. switches. Also driven by MIDI CC 1.");
    addAndMakeVisible (pitchWheel);
    addAndMakeVisible (modWheel);
    pitchWheel.setBounds (52, 594, 34, 128);
    modWheel.setBounds (110, 594, 34, 128);
    legend ("PITCH", { 44.0f, 728.0f, 50.0f, 11.0f }, 9.0f);
    legend ("MOD.", { 102.0f, 728.0f, 50.0f, 11.0f }, 9.0f);

    buildExtras();

    // ------------------------------------------------------------- keyboard
    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (41, 84); // 44 keys, F to C
    keyboard.setScrollButtonsVisible (false);
    keyboard.setBounds (174, 536, 1188, 208);
    keyboard.setKeyWidth (1188.0f / 26.0f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setBlackNoteWidthProportion (0.58f);
    keyboard.setBlackNoteLengthProportion (0.62f);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff3ecdc));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff121212));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0x55000000));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colour (0x40000000));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff6b655a));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::rockerOrange.withAlpha (0.15f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::rockerOrange.withAlpha (0.45f));

    startTimerHz (30);
}

Panel::~Panel()
{
    stopTimer();
    for (auto& s : sliders) s->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::Slider& Panel::knob (const char* paramId, juce::Point<int> centre, int size, const juce::String& kind,
                           const juce::String& lbls, int boxSize)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s->getProperties().set (props::scale, kind);
    s->getProperties().set (props::labels, lbls);
    s->getProperties().set (props::knobSize, size);
    const int box = std::max (size, boxSize);
    s->setBounds (juce::Rectangle<int> (box, box).withCentre (centre));
    s->setPopupDisplayEnabled (true, false, this);
    s->setMouseDragSensitivity (kind == "stepped" ? 120 : 220);
    s->setVelocityBasedMode (false);
    s->setTooltip (tooltipFor (paramId) + "\n(double-click to reset)");
    addAndMakeVisible (*s);
    sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, paramId, *s));
    if (auto* param = proc.state.getParameter (paramId))
        s->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    sliders.push_back (std::move (s));
    return *sliders.back();
}

juce::ToggleButton& Panel::rocker (const char* paramId, juce::Rectangle<int> area, const juce::String& colour)
{
    auto b = std::make_unique<juce::ToggleButton>();
    b->getProperties().set (props::rocker, colour);
    b->setBounds (area);
    b->setTooltip (tooltipFor (paramId));
    addAndMakeVisible (*b);
    buttonAtt.push_back (std::make_unique<ButtonAttachment> (proc.state, paramId, *b));
    buttons.push_back (std::move (b));
    return *buttons.back();
}

void Panel::legend (const juce::String& text, juce::Rectangle<float> area, float size, juce::Justification just, bool dim)
{
    legends.push_back ({ text, area, size, just, dim });
}

void Panel::knobLegend (const juce::String& text, juce::Point<int> centre, int size)
{
    legend (text, juce::Rectangle<float> (float (size) + 30.0f, 12.0f)
                      .withCentre ({ float (centre.x), float (centre.y) + float (size) * 0.5f + 4.0f }), 9.5f);
}

void Panel::heading (const juce::String& text, float x, float y, float lineEnd)
{
    headings.push_back ({ text, x, y, lineEnd });
}

void Panel::buildExtras()
{
    constexpr int capY = 464, y = 478, h = 30;

    auto caption = [&] (const juce::String& t, int x, int w) {
        auto l = std::make_unique<juce::Label> (juce::String(), t);
        l->setFont (PanelLookAndFeel::legendFont (9.5f));
        l->setJustificationType (juce::Justification::centredLeft);
        l->setColour (juce::Label::textColourId, colours::stripLabel);
        l->setBorderSize ({ 0, 1, 0, 0 });
        l->setBounds (x, capY, w, 12);
        addAndMakeVisible (*l);
        labels.push_back (std::move (l));
    };
    auto combo = [&] (const char* pid, int x, int w) {
        auto c = std::make_unique<juce::ComboBox>();
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter (pid)))
            c->addItemList (param->choices, 1);
        c->setBounds (x, y, w, h);
        c->setTooltip (tooltipFor (pid));
        addAndMakeVisible (*c);
        comboAtt.push_back (std::make_unique<ComboAttachment> (proc.state, pid, *c));
        combos.push_back (std::move (c));
    };
    auto incDec = [&] (const char* pid, int x, int w) {
        auto s = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
        s->setLookAndFeel (&lnf); // value box must be created by our look-and-feel (amber LCD)
        s->setColour (juce::Slider::textBoxTextColourId, colours::lcdText);
        s->setColour (juce::Slider::textBoxBackgroundColourId, colours::lcdBack);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, true, 52, h); // read-only LED display
        s->setIncDecButtonsMode (juce::Slider::incDecButtonsNotDraggable);
        s->setBounds (x, y, w, h);
        s->setTooltip (tooltipFor (pid));
        addAndMakeVisible (*s);
        sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, pid, *s));
        sliders.push_back (std::move (s));
    };

    caption ("PROGRAM", 52, 120);
    presetBox.setBounds (52, y, 196, h);
    for (int i = 0; i < mm::numFactoryPresets; ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 3) + " " + mm::factoryPresets[i].name, i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setTooltip ("Factory programs. Choosing one resets the panel (Unit, Quality and Drift are kept).");
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);
    prevProgram.setBounds (254, y + 2, 26, h - 4);
    nextProgram.setBounds (284, y + 2, 26, h - 4);
    prevProgram.setTooltip ("Previous program");
    nextProgram.setTooltip ("Next program");
    prevProgram.onClick = [this] { stepProgram (-1); };
    nextProgram.onClick = [this] { stepProgram (1); };
    addAndMakeVisible (prevProgram);
    addAndMakeVisible (nextProgram);

    caption ("VOICES", 334, 80);
    combo (id::voiceMode, 334, 84);
    caption ("POLY", 440, 80);
    incDec (id::voices, 440, 108);
    caption ("QUALITY", 570, 100);
    combo (id::quality, 570, 142);

    caption ("DRIFT", 732, 80);
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setBounds (732, y, 80, h);
        s->setPopupDisplayEnabled (true, false, this);
        s->setTooltip (tooltipFor (id::drift));
        addAndMakeVisible (*s);
        sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, id::drift, *s));
        sliders.push_back (std::move (s));
    }
    caption ("UNIT", 832, 80);
    incDec (id::unitSeed, 832, 114);
    caption ("BEND", 966, 80);
    incDec (id::bendRange, 966, 114);
    caption ("FEEDBACK", 1100, 70);
    rocker (id::feedback, { 1112, 476, 22, 38 }, "orange");
}

void Panel::stepProgram (int delta)
{
    const int n = mm::numFactoryPresets;
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
}

void Panel::timerCallback()
{
    // keep the program display's marquee moving when the name is too long
    if (led::needsScrolling (presetBox.getLocalBounds().toFloat().reduced (4.0f, 5.0f).withTrimmedRight (16.0f), presetBox.getText()))
        presetBox.repaint();
    if (pitchWheel.refresh()) pitchWheel.repaint();
    if (modWheel.refresh()) modWheel.repaint();
    const int program = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != program)
        presetBox.setSelectedId (program, juce::dontSendNotification);
}

void Panel::paintWood (juce::Graphics& g, juce::Rectangle<float> area, bool vertical) const
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    if (wood.isValid())
    {
        // Texture tile ~600 x 400 base units, grain along the length of the bar.
        const float scale = 0.5f;
        auto t = juce::AffineTransform::scale (scale);
        if (vertical)
            t = t.rotated (juce::MathConstants<float>::halfPi).translated (area.getRight(), area.getY());
        else
            t = t.translated (area.getX(), area.getY());
        g.setFillType (juce::FillType (wood, t));
        g.fillRect (area);
    }
    else
    {
        g.setColour (colours::woodLight);
        g.fillRect (area);
    }
    // Rounded profile: lighter in the middle, darker toward the edges.
    const auto a = vertical ? area.getTopLeft() : area.getTopLeft();
    const auto b = vertical ? area.getTopRight() : area.getBottomLeft();
    juce::ColourGradient profile (juce::Colours::black.withAlpha (0.35f), a, juce::Colours::black.withAlpha (0.45f), b, false);
    profile.addColour (0.35, juce::Colours::transparentBlack);
    profile.addColour (0.6, juce::Colours::white.withAlpha (0.04f));
    g.setGradientFill (profile);
    g.fillRect (area);
}

void Panel::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();

    // ---- wooden case: horizontal rails, then the two vertical cheeks
    paintWood (g, all, false);
    paintWood (g, all.withWidth (caseLeft), true);
    paintWood (g, all.withLeft (caseRight), true);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRect (all, 1.5f);

    // ---- recessed panels (drop shadow onto the wood + dark edge)
    auto recess = [&] (juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom) {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (r.expanded (2.0f));
        if (leather.isValid())
        {
            // Leather-grain covering; one continuous texture across the three panels.
            g.setFillType (juce::FillType (leather, juce::AffineTransform::scale (leatherScale).translated (caseLeft, 0.0f)));
            g.fillRect (r);
            // tone the grain down so legends stay readable, plus a slight vertical falloff
            juce::ColourGradient shade (juce::Colours::black.withAlpha (0.05f), r.getTopLeft(),
                                        juce::Colours::black.withAlpha (0.3f), r.getBottomLeft(), false);
            g.setGradientFill (shade);
            g.fillRect (r);
        }
        else
        {
            juce::ColourGradient face (top, r.getTopLeft(), bottom, r.getBottomLeft(), false);
            g.setGradientFill (face);
            g.fillRect (r);
        }
        // inner shadow at the top edge
        juce::ColourGradient shade (juce::Colours::black.withAlpha (0.5f), r.getTopLeft(),
                                    juce::Colours::transparentBlack, r.getTopLeft().translated (0.0f, 8.0f), false);
        g.setGradientFill (shade);
        g.fillRect (r.withHeight (8.0f));
    };
    recess (frontPanel, juce::Colour (0xff222224), juce::Colour (0xff1a1a1c));
    recess (settingsStrip, colours::strip, juce::Colour (0xff141416));
    recess (keyBed, colours::strip, juce::Colour (0xff121214));

    // ---- section separators and titles
    for (size_t i = 0; i < 5; ++i)
    {
        const auto& r = sectionX[i];
        if (i > 0)
        {
            g.setColour (colours::legend.withAlpha (0.25f));
            g.drawLine (r.getStart(), frontPanel.getY() + 10.0f, r.getStart(), frontPanel.getBottom(), 1.0f);
        }
        g.setColour (colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (21.0f));
        g.drawFittedText (sectionTitle[i],
                          juce::Rectangle<float> (r.getStart(), titleLine + 2.0f, r.getLength(), frontPanel.getBottom() - titleLine - 4.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    // ---- sub-headings with rules (FILTER, FILTER CONTOUR, LOUDNESS CONTOUR)
    for (const auto& hd : headings)
    {
        g.setColour (colours::legend);
        const auto font = PanelLookAndFeel::legendFont (10.5f);
        g.setFont (font);
        const float w = juce::GlyphArrangement::getStringWidth (font, hd.text);
        g.drawText (hd.text, juce::Rectangle<float> (hd.x, hd.y, w + 4.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (colours::legend.withAlpha (0.35f));
        g.drawLine (hd.x + w + 12.0f, hd.y + 6.0f, hd.lineEnd, hd.y + 6.0f, 1.0f);
    }

    // ---- left-hand controller well, keyboard frame
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRect (juce::Rectangle<float> (172.0f, 534.0f, 1192.0f, 212.0f), 2.0f);

    // ---- name plate
    // Brushed-metal plate, screwed onto the strip, with engraved lettering.
    const auto plate = juce::Rectangle<float> (1180.0f, 468.0f, 172.0f, 44.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (plate.translated (1.0f, 2.0f).expanded (1.0f), 3.5f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path outline;
        outline.addRoundedRectangle (plate, 3.0f);
        g.reduceClipRegion (outline);
        if (brushed.isValid())
        {
            const float scale = plate.getWidth() / float (brushed.getWidth());
            const float yOff = plate.getCentreY() - 0.5f * float (brushed.getHeight()) * scale;
            g.drawImageTransformed (brushed, juce::AffineTransform::scale (scale).translated (plate.getX(), yOff));
        }
        else
        {
            g.setColour (juce::Colour (0xff4a4a4c));
            g.fillRect (plate);
        }
        // sheen: light from above
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.12f), plate.getTopLeft(),
                                    juce::Colours::black.withAlpha (0.25f), plate.getBottomLeft(), false);
        g.setGradientFill (sheen);
        g.fillRect (plate);
    }
    // bevel
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawLine (plate.getX() + 3.0f, plate.getY() + 0.75f, plate.getRight() - 3.0f, plate.getY() + 0.75f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (plate, 3.0f, 1.0f);
    // two screws
    for (float sx : { plate.getX() + 9.0f, plate.getRight() - 9.0f })
    {
        const auto screw = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ sx, plate.getCentreY() });
        juce::ColourGradient head (juce::Colour (0xffd0d0cc), screw.getTopLeft(), juce::Colour (0xff5a5a58), screw.getBottomRight(), false);
        g.setGradientFill (head);
        g.fillEllipse (screw);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (screw, 0.6f);
        g.drawLine (screw.getX() + 1.2f, screw.getBottom() - 1.2f, screw.getRight() - 1.2f, screw.getY() + 1.2f, 0.8f);
    }
    // Engraved lettering filled with white paint. Light comes from above, so the
    // cut's lower lip catches a highlight under each letter, while the cut's
    // upper wall throws a shadow over the top of the paint inside the letter.
    auto engrave = [&] (const juce::String& text, juce::Rectangle<float> area, float height, juce::Colour paint) {
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (PanelLookAndFeel::legendFont (height), text, area.getX(), area.getY(),
                              area.getWidth(), area.getHeight(), juce::Justification::centred, 1);
        juce::Path letters;
        glyphs.createPath (letters);
        const bool large = height > 12.0f;

        // lower lip of the cut catches the light (thin, hard edge)
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillPath (letters, juce::AffineTransform::translation (0.0f, 0.9f));
        // soft darkening of the metal just above the cut
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 1, { 0, -1 }).drawForPath (g, letters);

        // paint fill
        g.setColour (paint);
        g.fillPath (letters);

        // soft inner shadow cast by the upper wall of the cut onto the paint
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (letters);
        // (a thin sharp band: a blur would grey out strokes this narrow)
        juce::Path outside;
        outside.addRectangle (area.expanded (12.0f));
        outside.setUsingNonZeroWinding (false);
        outside.addPath (letters, juce::AffineTransform::translation (0.0f, large ? 0.6f : 0.4f));
        g.setColour (juce::Colours::black.withAlpha (large ? 0.55f : 0.35f));
        g.fillPath (outside);
    };
    engrave ("mini moop", plate.withHeight (28.0f).translated (0.0f, 2.0f), 21.0f, juce::Colour (0xfff4f1e8));
    engrave ("CIRCUIT SIMULATION SYNTH", plate.withTop (plate.getY() + 27.0f).withHeight (13.0f), 8.5f, juce::Colour (0xffd9d5ca));

    // ---- legends
    for (const auto& l : legends)
    {
        g.setColour (l.dim ? colours::legendDim : colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (l.size));
        g.drawFittedText (l.text, l.area.toNearestInt(), l.just, 2, 0.8f);
    }
}

} // namespace mm::ui
