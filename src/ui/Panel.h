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
// The instrument front panel in a wooden case: CONTROLLERS, OSCILLATOR BANK,
// MIXER, MODIFIERS, OUTPUT; a settings strip with amber displays for the
// non-original settings; the left-hand controller (wheels, GLIDE / DECAY)
// and a 44-key keyboard (F to C).

#include "AboutBox.h"
#include "BankManager.h"
#include "PanelLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MiniMoopProcessor;

namespace mm::ui
{

class HelpBubble;

/** Performance wheel bound to an atomic float. Pitch wheel springs back.
    While hovered, shows a help bubble: "<name>: <value>", then a description. */
class Wheel : public juce::Component
{
public:
    Wheel (std::atomic<float>& target, bool bipolar, juce::String name);
    ~Wheel() override;
    /** `valueText` formats the wheel's value (-1..1 or 0..1) for the bubble. */
    void setHelp (juce::String description, std::function<juce::String (float)> valueText);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool refresh(); // returns true if the displayed value changed

private:
    void updateBubble();

    std::atomic<float>& value;
    bool bipolar;
    float shown = 0.0f, dragStartValue = 0.0f;
    juce::String description;
    std::function<juce::String (float)> valueText;
    std::unique_ptr<HelpBubble> bubble;
};

/** Panel lamp (OVERLOAD, main output): chrome bezel and coloured lens, lit
    from an atomic brightness (0..1) published by the audio thread or a
    parameter. */
class Lamp : public juce::Component
{
public:
    /** Lens colours: unlit, lit, and the filament's hot centre. */
    struct Lens { juce::Colour dark, lit, hot; };
    static inline const Lens red { juce::Colour (0xff3c0f0b), juce::Colour (0xffff5530), juce::Colour (0xffffd2a0) };
    static inline const Lens green { juce::Colour (0xff0e2a10), juce::Colour (0xff46e05a), juce::Colour (0xffd4ffc8) };

    Lamp (std::atomic<float>& level, Lens lensColours) : brightness (level), lens (lensColours), shown (level.load()) {}
    void paint (juce::Graphics&) override;
    bool refresh(); // returns true if the displayed brightness changed

private:
    std::atomic<float>& brightness;
    Lens lens;
    float shown = 0.0f;
};

/** Keyboard with cream keytops and glossy black sharps. */
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard) {}

protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};

class Panel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseWidth = 1400;
    static constexpr int baseHeight = 768;

    explicit Panel (MiniMoopProcessor&);
    ~Panel() override;

    void showAbout() { about.show(); }
    void showBanks() { banks.show(); }

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Legend { juce::String text; juce::Rectangle<float> area; float size; juce::Justification just; bool dim; };

    /** `size` is the nominal knob size (same knob diameter for equal sizes);
        `boxSize` > size leaves room for legends outside the scale. */
    juce::Slider& knob (const char* paramId, juce::Point<int> centre, int size, const juce::String& kind = "pot",
                        const juce::String& labels = {}, int boxSize = 0);
    juce::ToggleButton& rocker (const char* paramId, juce::Rectangle<int> area, const juce::String& colour);
    void legend (const juce::String& text, juce::Rectangle<float> area, float size = 11.0f,
                 juce::Justification just = juce::Justification::centred, bool dim = false);
    void knobLegend (const juce::String& text, juce::Point<int> centre, int size);
    void heading (const juce::String& text, float x, float y, float lineEnd);
    void buildExtras();
    void paintWood (juce::Graphics&, juce::Rectangle<float> area, bool vertical) const;
    void stepProgram (int delta);
    void showMenu();
    void rebuildProgramList (std::shared_ptr<const SoundBank> bank);
    void syncProgramDisplay();
    void timerCallback() override;

    MiniMoopProcessor& proc;
    PanelLookAndFeel lnf;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAtt;
    std::vector<std::unique_ptr<ComboAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::MouseListener>> hoverHelps; // cleared first in ~Panel()
    std::vector<Legend> legends;
    struct Heading { juce::String text; float x, y, lineEnd; };
    std::vector<Heading> headings;
    std::vector<juce::Line<float>> wires; // painted lines on the panel

    juce::Image wood, leather, brushed;
    juce::ComboBox presetBox;
    juce::TextButton prevProgram { "<" }, nextProgram { ">" };
    Wheel pitchWheel, modWheel;
    Lamp overload, mainLamp;
    Keyboard keyboard;
    BurgerButton burger;
    AboutBox about { wood };
    BankManager banks { proc };
    std::shared_ptr<const SoundBank> programListBank; // bank shown in presetBox
};

} // namespace mm::ui
