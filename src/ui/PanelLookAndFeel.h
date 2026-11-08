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
// Vector-drawn panel hardware: skirted knobs with 0..10 scales, rotary
// selector switches with legends, rocker switches. No bitmaps.

#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::ui
{

namespace colours
{
inline const juce::Colour panel { 0xff1b1b1d };
inline const juce::Colour panelEdge { 0xff2a2a2e };
inline const juce::Colour legend { 0xffe9e6dc };
inline const juce::Colour legendDim { 0xffa7a49a };
inline const juce::Colour woodDark { 0xff4a2a14 };
inline const juce::Colour woodLight { 0xff8a5630 };
inline const juce::Colour rockerBlue { 0xff2f74c0 };
inline const juce::Colour rockerOrange { 0xffe07b22 };
inline const juce::Colour rockerWhite { 0xffe8e4d8 };
inline const juce::Colour knobSkirt { 0xff0d0d0e };
inline const juce::Colour knobCap { 0xffb9b9b4 };
inline const juce::Colour strip { 0xff19191b };          // settings strip / keyboard bed
inline const juce::Colour lcdBack { 0xff0f0c08 };
inline const juce::Colour lcdText { 0xfff0a536 };        // amber display text
inline const juce::Colour stripLabel { 0xffc9b48f };     // small tan captions
} // namespace colours

/** Slider property keys understood by the look-and-feel. */
namespace props
{
inline constexpr const char* scale = "mmScale";      // "pot" (0..10), "bipolar", "stepped"
inline constexpr const char* labels = "mmLabels";    // '|' separated legends for stepped/bipolar
inline constexpr const char* rocker = "mmRocker";    // rocker colour for toggle buttons: blue/orange/white
inline constexpr const char* knobSize = "mmKnobSize"; // nominal knob size when the component box is larger
} // namespace props

class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static juce::Font legendFont (float height, bool bold = true);
    static juce::Font lcdFont (float height);
    /** Recessed amber "LCD" window. */
    static void drawLcdWindow (juce::Graphics&, juce::Rectangle<float> area);

    /** Draws a small waveform pictogram for the oscillator selectors. */
    static void drawWaveIcon (juce::Graphics&, juce::Rectangle<float> area, const juce::String& token);
};

} // namespace mm::ui
