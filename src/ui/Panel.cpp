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
#include "AboutBox.h"
#include "LedDisplay.h"
#include "Tooltips.h"

#include "MiniMoopAssets.h"

namespace mm::ui
{

namespace id = mm::params::id;

// ============================================================================ HelpBubble

/** Hover help of the switches, wheels and settings, drawn as the knobs' value
    bubble (same bubble, colours and bold font): "<name>: <state>", then the
    description wrapped below it. An empty title leaves just the description. */
class HelpBubble : public juce::BubbleComponent
{
public:
    /** `margin`: room left between the control and the arrow's tip. */
    explicit HelpBubble (juce::Component& switchToDescribe, int margin = 0) : owner (switchToDescribe), tipMargin (margin)
    {
        setAlwaysOnTop (true);
    }

    void setText (const juce::String& newTitle, const juce::String& newDescription)
    {
        auto& lf = owner.getLookAndFeel();
        const juce::Font titleFont (lf.withDefaultMetrics (juce::FontOptions (15.0f, juce::Font::bold)));
        const juce::Font bodyFont (lf.withDefaultMetrics (juce::FontOptions (13.0f)));
        const auto colour = owner.findColour (juce::TooltipWindow::textColourId, true);

        juce::AttributedString text;
        if (newTitle.isNotEmpty())
            text.append (newTitle, titleFont, colour);
        if (newDescription.isNotEmpty())
            text.append ((newTitle.isNotEmpty() ? "\n" : "") + newDescription, bodyFont, colour);

        // As wide as the longest line, wrapped beyond maxWidth.
        float natural = newTitle.isNotEmpty() ? juce::GlyphArrangement::getStringWidth (titleFont, newTitle) : 0.0f;
        if (newDescription.isNotEmpty())
            natural = std::max (natural, juce::GlyphArrangement::getStringWidth (bodyFont, newDescription));
        layout.createLayout (text, std::ceil (std::min (natural, maxWidth)) + 1.0f);

        if (auto* parent = getParentComponent(); parent != nullptr && tipMargin > 0)
            BubbleComponent::setPosition (parent->getLocalArea (&owner, owner.getLocalBounds()).expanded (tipMargin));
        else
            BubbleComponent::setPosition (&owner);
        repaint();
    }

    void getContentSize (int& w, int& h) override
    {
        w = int (std::ceil (layout.getWidth())) + 2 * padX;
        h = int (std::ceil (layout.getHeight())) + 2 * padY;
    }

    void paintContent (juce::Graphics& g, int w, int h) override
    {
        layout.draw (g, juce::Rectangle<int> (w, h).reduced (padX, padY).toFloat());
    }

private:
    static constexpr float maxWidth = 300.0f;
    static constexpr int padX = 9, padY = 5;
    juce::Component& owner;
    int tipMargin;
    juce::TextLayout layout;
};

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
    updateBubble();
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

Wheel::~Wheel() = default;

void Wheel::setHelp (juce::String newDescription, std::function<juce::String (float)> newValueText)
{
    description = std::move (newDescription);
    valueText = std::move (newValueText);
}

void Wheel::mouseEnter (const juce::MouseEvent&)
{
    auto* parent = getParentComponent();
    if (parent == nullptr) return;
    if (bubble == nullptr)
    {
        bubble = std::make_unique<HelpBubble> (*this);
        parent->addChildComponent (*bubble);
    }
    updateBubble();
    bubble->setVisible (true);
}

void Wheel::mouseExit (const juce::MouseEvent&) { bubble.reset(); }

void Wheel::updateBubble()
{
    if (bubble != nullptr)
        bubble->setText (getName() + ": " + (valueText != nullptr ? valueText (shown) : juce::String (shown, 2)), description);
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

// ============================================================================ Lamp

bool Lamp::refresh()
{
    const float v = brightness.load();
    if (std::abs (v - shown) < 0.01f) return false;
    shown = v;
    return true;
}

void Lamp::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float rb = std::min (r.getWidth(), r.getHeight()) * 0.5f - 1.0f; // bezel
    const float rl = rb * 0.62f;                                              // lens
    auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

    // chrome bezel
    juce::ColourGradient chrome (juce::Colour (0xffe8e8e4), c.translated (-rb, -rb), juce::Colour (0xff4a4a48), c.translated (rb, rb), false);
    chrome.addColour (0.55, juce::Colour (0xff9a9a96));
    g.setGradientFill (chrome);
    g.fillEllipse (circle (rb));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (circle (rb), 0.8f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (circle (rl + 1.2f));

    // coloured lens, lit by the filament (perceived brightness rises fast at first)
    const float b = std::pow (juce::jlimit (0.0f, 1.0f, shown), 0.6f);
    const auto dark = lens.dark, lit = lens.lit;
    juce::ColourGradient glass (dark.interpolatedWith (lens.hot, b).brighter (0.1f), c.translated (-rl * 0.3f, -rl * 0.4f),
                                dark.interpolatedWith (lit, b).darker (0.3f * (1.0f - b)), c.translated (rl, rl), true);
    g.setGradientFill (glass);
    g.fillEllipse (circle (rl));
    if (b > 0.01f)
    {
        g.setColour (lit.withAlpha (0.35f * b));
        g.drawEllipse (circle (rl + 0.6f), 1.2f);
    }
    // specular highlight
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (rl * 0.55f, rl * 0.4f).withCentre (c.translated (-rl * 0.3f, -rl * 0.45f)));
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
const juce::Range<float> sectionX[] = { { 34, 234 }, { 234, 614 }, { 614, 912 }, { 912, 1256 }, { 1256, 1366 } };
// OSCILLATOR MODULATION rocker, centred on the CONTROLLERS / OSCILLATOR BANK border.
const juce::Rectangle<float> oscModSwitch { 209.0f, 98.0f, 50.0f, 24.0f };
// FILTER MODULATION and KEYBOARD CONTROL rockers, centred on the MIXER / MODIFIERS border.
constexpr float borderSwitchX = 912.0f;
// Each section border is interrupted (vertical range) around the switches and legends sitting on it.
const juce::Range<float> borderGap[] = { {}, { 62.0f, 140.0f }, {}, { 58.0f, 240.0f }, {} };
const char* sectionTitle[] = { "CONTROLLERS", "OSCILLATOR BANK", "MIXER", "MODIFIERS", "OUTPUT" };

/** Rocker switch showing its help bubble while hovered (instead of a
    tooltip); the bubble follows the switch's state. */
class PanelSwitch : public juce::ToggleButton
{
public:
    PanelSwitch (const juce::String& helpId, juce::Component& bubbleParent)
        : help (switchHelpFor (helpId)), parent (bubbleParent) {}

    void mouseEnter (const juce::MouseEvent& e) override
    {
        ToggleButton::mouseEnter (e);
        if (bubble == nullptr)
        {
            bubble = std::make_unique<HelpBubble> (*this);
            parent.addChildComponent (*bubble);
        }
        updateBubble();
        bubble->setVisible (true);
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        ToggleButton::mouseExit (e);
        bubble.reset();
    }

protected:
    void buttonStateChanged() override
    {
        ToggleButton::buttonStateChanged();
        if (bubble != nullptr) updateBubble();
    }

private:
    void updateBubble()
    {
        bubble->setText (juce::String (help.name) + ": " + (getToggleState() ? help.on : help.off),
                         help.description);
    }

    SwitchHelp help;
    juce::Component& parent;
    std::unique_ptr<HelpBubble> bubble;
};

/** Help bubble for any control (settings strip faders and boxes), shown
    while the mouse is over it or its parts. `title` may be null (description
    only); call refresh() when what it shows changes. */
class HoverHelp : public juce::MouseListener
{
public:
    HoverHelp (juce::Component& c, std::function<juce::String()> titleText, juce::String desc, int tipMargin = 0)
        : target (c), title (std::move (titleText)), description (std::move (desc)), margin (tipMargin)
    {
        target.addMouseListener (this, true);
    }

    ~HoverHelp() override { target.removeMouseListener (this); }

    void refresh()
    {
        if (bubble != nullptr)
            bubble->setText (title != nullptr ? title() : juce::String(), description);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        if (bubble != nullptr) return;
        auto* parent = target.getParentComponent();
        if (parent == nullptr) return;
        bubble = std::make_unique<HelpBubble> (target, margin);
        parent->addChildComponent (*bubble);
        refresh();
        bubble->setVisible (true);
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        // Moving onto one of the control's own parts (a box's label) is not leaving it.
        if (! target.getLocalBounds().contains (e.getEventRelativeTo (&target).getPosition()))
            bubble.reset();
    }

private:
    juce::Component& target;
    std::function<juce::String()> title;
    juce::String description;
    int margin;
    std::unique_ptr<HelpBubble> bubble;
};
} // namespace

Panel::Panel (MiniMoopProcessor& p)
    : proc (p),
      pitchWheel (p.pitchWheel, true, "PITCH WHEEL"),
      modWheel (p.modWheel, false, "MOD. WHEEL"),
      overload (p.overloadLamp, Lamp::red),
      mainLamp (*p.state.getRawParameterValue (id::mainOn), Lamp::green),
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
    // Legends above the knobs, as on the original panel.
    auto legendAbove = [this] (const juce::String& text, juce::Point<int> centre, int size) {
        legend (text, juce::Rectangle<float> (float (size) + 40.0f, 12.0f)
                          .withCentre ({ float (centre.x), float (centre.y) - float (size) * 0.5f - 14.0f }), 9.5f);
    };
    // Rows placed so that the block (TUNE legend to the reissue switch legends)
    // is centred vertically between the top of the panel and the section title.
    const juce::Point<int> tunePos { 122, 134 }, glidePos { 86, 254 }, modMixPos { 182, 254 };
    knob (id::tune, tunePos, 92, "bipolar", juce::String::fromUTF8 ("\xe2\x88\x92" "2|\xe2\x88\x92" "1|0|1|2"));
    legendAbove ("TUNE", tunePos, 92);
    knob (id::glide, glidePos, 92);
    legendAbove ("GLIDE", glidePos, 92);
    knob (id::modMix, modMixPos, 92);
    legendAbove ("MODULATION MIX", modMixPos, 92);
    legend ("OSC. 3", { float (modMixPos.x) - 52.0f, float (modMixPos.y) + 39.0f, 44.0f, 11.0f }, 9.0f);
    legend ("NOISE", { float (modMixPos.x) + 8.0f, float (modMixPos.y) + 39.0f, 44.0f, 11.0f }, 9.0f);

    // OSCILLATOR MODULATION straddles the CONTROLLERS / OSCILLATOR BANK border.
    rocker (id::oscMod, oscModSwitch.toNearestInt(), "orange").getProperties().set (props::rockerHorizontal, true);
    legend ("OSCILLATOR\nMODULATION", { oscModSwitch.getCentreX() - 45.0f, oscModSwitch.getY() - 28.0f, 90.0f, 24.0f }, 9.0f);
    legend ("ON", { oscModSwitch.getCentreX(), oscModSwitch.getBottom() + 3.0f, oscModSwitch.getWidth() * 0.5f, 11.0f }, 9.0f);

    // Modulation source switches of the modern reissues: shown, but inactive.
    auto reissueSwitch = [this] (const char* tipId, juce::Rectangle<int> area, const char* leftText, const char* rightText) {
        auto b = std::make_unique<PanelSwitch> (tipId, *this);
        b->getProperties().set (props::rocker, "orange");
        b->getProperties().set (props::rockerHorizontal, true);
        b->getProperties().set (props::rockerInactive, true);
        b->setClickingTogglesState (false); // kept enabled so that its help still shows
        b->setBounds (area);
        addAndMakeVisible (*b);
        buttons.push_back (std::move (b));
        const auto r = area.toFloat();
        legend (leftText, { r.getX() - 16.0f, r.getBottom() + 3.0f, r.getWidth() * 0.5f + 16.0f, 11.0f }, 9.0f,
                juce::Justification::centred, true);
        legend (rightText, { r.getCentreX(), r.getBottom() + 3.0f, r.getWidth() * 0.5f + 16.0f, 11.0f }, 9.0f,
                juce::Justification::centred, true);
    };
    reissueSwitch ("modSourceA", { glidePos.x - 28, glidePos.y + 66, 56, 24 }, "OSC. 3", "FILTER EG");
    reissueSwitch ("modSourceB", { modMixPos.x - 28, modMixPos.y + 66, 56, 24 }, "NOISE", "LFO");

    // -------------------------------------------------------- OSCILLATOR BANK
    const float colX[3] = { 330.0f, 440.0f, 552.0f };
    legend ("RANGE", { colX[0] - 50.0f, 40.0f, 100.0f, 13.0f }, 10.5f);
    // Oscillator names as on the original panel: OSCILLATOR-1 over the FREQUENCY
    // heading (oscillator 1 has no frequency pot), OSCILLATOR-2/3 above their pots.
    auto oscName = [] (int n) { return juce::String::fromUTF8 ("OSCILLATOR\xe2\x80\x94") + juce::String (n); };
    legend (oscName (1), { colX[1] - 60.0f, 33.0f, 120.0f, 13.0f }, 10.5f);
    legend ("FREQUENCY", { colX[1] - 50.0f, 47.0f, 100.0f, 13.0f }, 10.5f);
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
            legendAbove (oscName (i + 1), { int (colX[1]), rowY[i] }, 92);
        }
    }
    rocker (id::osc3Control, { 244, 310, 24, 44 }, "orange");
    legend ("OSC. 3\nCTRL", { 226.0f, 282.0f, 60.0f, 24.0f }, 9.0f); // above the switch

    // ------------------------------------------------------------------ MIXER
    // Original layout: oscillator volumes on the left (rows 1, 3, 5), a column of
    // ON switches, EXTERNAL INPUT and NOISE volumes on the right (rows 2, 4),
    // joined to their switches by painted lines; OVERLOAD lamp at the top.
    const int mixY[5] = { 98, 163, 228, 293, 358 };
    constexpr int volX = 672, switchX = 732, rightX = 840, mixKnob = 72;
    const char* mixVol[5] = { id::oscVol[0], id::extVol, id::oscVol[1], id::noiseVol, id::oscVol[2] };
    const char* mixOn[5] = { id::oscOn[0], id::extOn, id::oscOn[1], id::noiseOn, id::oscOn[2] };
    legend ("VOLUME", { float (volX) - 40.0f, 40.0f, 80.0f, 13.0f }, 10.5f);
    for (int i = 0; i < 5; ++i)
    {
        const bool right = i == 1 || i == 3; // EXT. INPUT and NOISE
        const float y = float (mixY[i]);
        knob (mixVol[i], { right ? rightX : volX, mixY[i] }, mixKnob);
        rocker (mixOn[i], { switchX, mixY[i] - 11, 44, 22 }, "blue").getProperties().set (props::rockerHorizontal, true);
        legend ("ON", { float (switchX) + 22.0f, y + 13.0f, 22.0f, 11.0f }, 9.0f);
        if (right)
            wires.push_back ({ float (switchX) + 45.0f, y, float (rightX) - 41.0f, y });
        else
            wires.push_back ({ float (volX) + 42.0f, y, float (switchX) - 1.0f, y });
    }
    legend ("EXTERNAL\nINPUT VOLUME", { float (rightX) - 45.0f, float (mixY[1]) - 62.0f, 90.0f, 24.0f }, 9.0f);
    legend ("NOISE VOLUME", { float (rightX) - 45.0f, float (mixY[3]) - 56.0f, 90.0f, 12.0f }, 9.0f);
    legend ("OVERLOAD", { float (rightX) - 40.0f, 40.0f, 80.0f, 13.0f }, 9.0f);
    overload.setBounds (juce::Rectangle<int> (26, 26).withCentre ({ rightX, 68 }));
    hoverHelps.push_back (std::make_unique<HoverHelp> (overload, [] { return juce::String ("OVERLOAD"); }, tooltipFor ("overload"),
                                                       6)); // the arrow stops short of the lamp
    addAndMakeVisible (overload);
    rocker (id::noiseColor, { rightX - 11, 340, 22, 36 }, "blue");
    legend ("WHITE", { float (rightX) - 30.0f, 327.0f, 60.0f, 11.0f }, 9.0f);
    legend ("PINK", { float (rightX) - 30.0f, 379.0f, 60.0f, 11.0f }, 9.0f);

    // FILTER MODULATION and KEYBOARD CONTROL 1 / 2 straddle the MIXER / MODIFIERS border.
    auto borderSwitch = [this] (const char* pid, float top) {
        rocker (pid, juce::Rectangle<float> (borderSwitchX - 25.0f, top, 50.0f, 24.0f).toNearestInt(), "orange")
            .getProperties().set (props::rockerHorizontal, true);
    };
    // The top switch is centred on the CUTOFF knob (y = 102).
    legend ("FILTER\nMODULATION", { borderSwitchX - 45.0f, 64.0f, 90.0f, 24.0f }, 9.0f);
    borderSwitch (id::filterMod, 90.0f);
    legend ("ON", { borderSwitchX, 117.0f, 25.0f, 11.0f }, 9.0f);
    legend ("ON", { borderSwitchX, 131.0f, 25.0f, 11.0f }, 9.0f);
    borderSwitch (id::kbd1, 144.0f);
    legend ("KEYBOARD\nCONTROL", { borderSwitchX - 45.0f, 171.0f, 90.0f, 24.0f }, 9.0f);
    borderSwitch (id::kbd2, 197.0f);
    legend ("ON", { borderSwitchX, 224.0f, 25.0f, 11.0f }, 9.0f);
    // KEYBOARD CONTROL 1 / 2 numbers, right of their switches.
    legend ("1", { borderSwitchX + 25.0f, 150.0f, 10.0f, 12.0f }, 10.0f);
    legend ("2", { borderSwitchX + 25.0f, 203.0f, 10.0f, 12.0f }, 10.0f);

    // -------------------------------------------------------------- MODIFIERS
    constexpr float headingX = 964.0f; // clear of the border switches
    heading ("FILTER", headingX, 40.0f, 1244.0f);
    const float modX[3] = { 998.0f, 1096.0f, 1194.0f };
    // Scale -5..+5 as on the original panel, numbered every 2 (-4 -2 0 2 4).
    knob (id::cutoff, { int (modX[0]), 102 }, 92, "custom",
          juce::String::fromUTF8 ("0=|0.1=\xe2\x88\x92" "4|0.2=|0.3=\xe2\x88\x92" "2|0.4=|0.5=0|0.6=|0.7=2|0.8=|0.9=4|1="));
    knobLegend ("CUTOFF FREQUENCY", { int (modX[0]), 102 }, 92);
    knob (id::emphasis, { int (modX[1]), 102 }, 92);
    knobLegend ("EMPHASIS", { int (modX[1]), 102 }, 92);
    knob (id::contourAmt, { int (modX[2]), 102 }, 92);
    knobLegend ("AMOUNT OF CONTOUR", { int (modX[2]), 102 }, 92);

    heading ("FILTER CONTOUR", headingX, 174.0f, 1244.0f);
    heading ("LOUDNESS CONTOUR", headingX, 290.0f, 1244.0f);
    const char* fEnv[3] = { id::fAttack, id::fDecay, id::fSustain };
    const char* lEnv[3] = { id::lAttack, id::lDecay, id::lSustain };
    const char* envName[3] = { "ATTACK TIME", "DECAY TIME", "SUSTAIN LEVEL" };
    // Attack / decay time scale of the original panel (calibrated, irregular ticks).
    const juce::String timeScale = "0=M-SEC.|0.08=10|0.12=|0.165=200|0.28=|0.385=600|0.5=|0.615=1|0.68=|0.735=5|0.815=|0.89=10|1=SEC.";
    for (int i = 0; i < 3; ++i)
    {
        const bool time = i < 2;
        knob (fEnv[i], { int (modX[i]), 228 }, 80, time ? "custom" : "pot", time ? timeScale : juce::String());
        knobLegend (envName[i], { int (modX[i]), 228 }, 80);
        knob (lEnv[i], { int (modX[i]), 344 }, 80, time ? "custom" : "pot", time ? timeScale : juce::String());
        knobLegend (envName[i], { int (modX[i]), 344 }, 80);
    }

    // ----------------------------------------------------------------- OUTPUT
    constexpr int outX = 1311; // centre of the OUTPUT section
    knob (id::volume, { outX, 88 }, 92);
    knobLegend ("VOLUME", { outX, 88 }, 92);
    rocker (id::mainOn, { outX - 12, 192, 24, 46 }, "blue");
    // Lamp showing whether the main output is on, right of its switch.
    mainLamp.setBounds (juce::Rectangle<int> (20, 20).withCentre ({ outX + 32, 215 }));
    addAndMakeVisible (mainLamp);
    legend ("ON", { float (outX) - 30.0f, 242.0f, 60.0f, 12.0f }, 9.5f);
    rocker (id::a440, { outX - 12, 296, 24, 46 }, "orange");
    legend ("A-440", { float (outX) - 30.0f, 346.0f, 60.0f, 12.0f }, 9.5f);

    // ------------------------------------------------- left-hand controller
    rocker (id::glideOn, { 58, 538, 22, 38 }, "orange");
    legend ("GLIDE", { 44.0f, 578.0f, 50.0f, 11.0f }, 9.0f);
    rocker (id::decayOn, { 116, 538, 22, 38 }, "blue");
    legend ("DECAY", { 102.0f, 578.0f, 50.0f, 11.0f }, 9.0f);
    pitchWheel.setHelp ("Bends all oscillators (range set by BEND). Springs back to centre. Also driven by MIDI pitch bend.",
                        [this] (float v) {
                            const float semis = v * proc.state.getRawParameterValue (id::bendRange)->load();
                            return (semis > 0.005f ? "+" : "") + juce::String (semis, 2) + " st";
                        });
    modWheel.setHelp ("Amount of modulation sent through the OSC. MOD. / FILTER MOD. switches. Also driven by MIDI CC 1. MOD. AMT sets a minimum kept with the sound.",
                      [] (float v) { return juce::String (v * 10.0f, 1); });
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

    // Overlays (hidden until chosen from the burger menu)
    about.setBounds (getLocalBounds());
    addChildComponent (about);
    banks.setBounds (getLocalBounds());
    addChildComponent (banks);

    startTimerHz (30);
}

Panel::~Panel()
{
    stopTimer();
    hoverHelps.clear(); // before the controls they watch (some are declared after them)
    for (auto& s : sliders) s->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

namespace
{
/** Rotary knob whose component box is larger than the knob (room for the
    scale legends) but which only takes mouse clicks on the knob itself, so
    that it never steals clicks from its neighbours. */
class PanelKnob : public juce::Slider
{
public:
    PanelKnob (float knobSize) : juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox), grabRadius (knobSize * 0.45f) {}

    bool hitTest (int x, int y) override
    {
        return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ float (x), float (y) }) <= grabRadius;
    }

private:
    float grabRadius;
};
} // namespace

juce::Slider& Panel::knob (const char* paramId, juce::Point<int> centre, int size, const juce::String& kind,
                           const juce::String& lbls, int boxSize)
{
    auto s = std::make_unique<PanelKnob> (float (size));
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s->getProperties().set (props::scale, kind);
    s->getProperties().set (props::labels, lbls);
    s->getProperties().set (props::knobSize, size);
    // The scale legends reach beyond the knob: give the component room for them.
    const int box = std::max (int (std::ceil (float (size) * 1.3f)), boxSize);
    s->setBounds (juce::Rectangle<int> (box, box).withCentre (centre));
    // No help tooltip on knobs: hovering shows the value bubble instead (the
    // one shown while dragging), kept until the mouse leaves the knob.
    s->setPopupDisplayEnabled (true, true, this, -1);
    s->setMouseDragSensitivity (kind == "stepped" ? 120 : 220);
    s->setVelocityBasedMode (false);
    addAndMakeVisible (*s);
    sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, paramId, *s));
    if (auto* param = proc.state.getParameter (paramId))
        s->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    sliders.push_back (std::move (s));
    return *sliders.back();
}

juce::ToggleButton& Panel::rocker (const char* paramId, juce::Rectangle<int> area, const juce::String& colour)
{
    auto b = std::make_unique<PanelSwitch> (paramId, *this);
    b->getProperties().set (props::rocker, colour);
    b->setBounds (area);
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
        hoverHelps.push_back (std::make_unique<HoverHelp> (*c, nullptr, tooltipFor (pid)));
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
        hoverHelps.push_back (std::make_unique<HoverHelp> (*s, nullptr, tooltipFor (pid)));
        addAndMakeVisible (*s);
        sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, pid, *s));
        sliders.push_back (std::move (s));
    };

    burger.setBounds (52, y, 30, h);
    burger.onClick = [this] { showMenu(); };
    addAndMakeVisible (burger);

    caption ("PROGRAM", 90, 120);
    presetBox.setBounds (90, y, 196, h);
    syncProgramDisplay();
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getLoadedProgram()) proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);
    prevProgram.setBounds (292, y + 2, 26, h - 4);
    nextProgram.setBounds (322, y + 2, 26, h - 4);
    prevProgram.onClick = [this] { stepProgram (-1); };
    nextProgram.onClick = [this] { stepProgram (1); };
    addAndMakeVisible (prevProgram);
    addAndMakeVisible (nextProgram);

    caption ("VOICES", 372, 80);
    combo (id::voiceMode, 372, 84);
    caption ("POLY", 474, 80);
    incDec (id::voices, 474, 108);
    caption ("QUALITY", 598, 100);
    combo (id::quality, 598, 122);

    // Faders: the help bubble shows "<name>: <value>" and follows the value.
    auto fader = [&] (const char* pid, const juce::String& name, int x, int w) {
        auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setBounds (x, y, w, h);
        auto* sp = s.get();
        auto help = std::make_unique<HoverHelp> (*s, [sp, name] { return name + ": " + sp->getTextFromValue (sp->getValue()); },
                                                 tooltipFor (pid));
        s->onValueChange = [hp = help.get()] { hp->refresh(); };
        hoverHelps.push_back (std::move (help));
        addAndMakeVisible (*s);
        sliderAtt.push_back (std::make_unique<SliderAttachment> (proc.state, pid, *s));
        sliders.push_back (std::move (s));
    };
    caption ("DRIFT", 736, 70);
    fader (id::drift, "DRIFT", 736, 70);
    caption ("MOD. AMT", 822, 70);
    fader (id::modAmount, "MOD. AMT", 822, 70);
    caption ("UNIT", 908, 80);
    incDec (id::unitSeed, 908, 108);
    caption ("BEND", 1032, 80);
    incDec (id::bendRange, 1032, 108);
    caption ("FEEDBACK", 1156, 56);
    rocker (id::feedback, { 1168, 476, 22, 38 }, "orange");
}

void Panel::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addItem ("Sound Banks...", [this] { banks.show(); });
    menu.addSeparator();
    menu.addItem ("About Mini Moop", [this] { about.show(); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&burger));
}

void Panel::rebuildProgramList (std::shared_ptr<const SoundBank> bank)
{
    programListBank = std::move (bank);
    presetBox.clear (juce::dontSendNotification);
    int flat = 0;
    for (const auto& category : programListBank->categories)
    {
        presetBox.addSectionHeading (category.name);
        for (const auto& preset : category.presets)
        {
            presetBox.addItem (juce::String (flat + 1).paddedLeft ('0', 3) + " " + preset.name, flat + 1);
            ++flat;
        }
    }
}

void Panel::syncProgramDisplay()
{
    if (auto bank = proc.getActiveBank(); bank != programListBank)
        rebuildProgramList (std::move (bank));
    const int program = proc.getLoadedProgram() + 1;
    if (program > 0)
    {
        if (presetBox.getSelectedId() != program)
            presetBox.setSelectedId (program, juce::dontSendNotification);
    }
    else
    {
        // sound not from the active bank (bank just changed): show the bank name
        const auto placeholder = "-- " + programListBank->name;
        if (presetBox.getSelectedId() != 0 || presetBox.getText() != placeholder)
            presetBox.setText (placeholder, juce::dontSendNotification);
    }
}

void Panel::stepProgram (int delta)
{
    const int n = proc.getActiveBank()->numPresets();
    if (n == 0) return;
    const int current = proc.getLoadedProgram();
    proc.setCurrentProgram (current < 0 ? (delta > 0 ? 0 : n - 1) : (current + delta + n) % n);
}

void Panel::timerCallback()
{
    // keep the program display's marquee moving when the name is too long
    if (led::needsScrolling (presetBox.getLocalBounds().toFloat().reduced (4.0f, 5.0f).withTrimmedRight (16.0f), presetBox.getText()))
        presetBox.repaint();
    if (pitchWheel.refresh()) pitchWheel.repaint();
    if (overload.refresh()) overload.repaint();
    if (mainLamp.refresh()) mainLamp.repaint();
    if (modWheel.refresh()) modWheel.repaint();
    syncProgramDisplay();
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
            const float x = r.getStart(), top = frontPanel.getY() + 10.0f, bottom = frontPanel.getBottom();
            const auto gap = borderGap[i];
            if (gap.isEmpty())
                g.drawLine (x, top, x, bottom, 1.0f);
            else
            {
                if (gap.getStart() > top) g.drawLine (x, top, x, gap.getStart(), 1.0f);
                g.drawLine (x, gap.getEnd(), x, bottom, 1.0f);
            }
        }
        g.setColour (colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (21.0f));
        g.drawFittedText (sectionTitle[i],
                          juce::Rectangle<float> (r.getStart(), titleLine + 2.0f, r.getLength(), frontPanel.getBottom() - titleLine - 4.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    // ---- painted lines joining mixer switches to their volume pots
    g.setColour (colours::legend);
    for (const auto& w : wires)
        g.drawLine (w, 2.0f);

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
    const auto plate = juce::Rectangle<float> (1212.0f, 468.0f, 140.0f, 44.0f);
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
    engrave ("CIRCUIT SIMULATION SYNTH", plate.withTop (plate.getY() + 27.0f).withHeight (13.0f).reduced (14.0f, 0.0f), 7.5f, juce::Colour (0xffd9d5ca));

    // ---- legends
    for (const auto& l : legends)
    {
        g.setColour (l.dim ? colours::legendDim : colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (l.size));
        g.drawFittedText (l.text, l.area.toNearestInt(), l.just, 2, 0.8f);
    }
}

} // namespace mm::ui
