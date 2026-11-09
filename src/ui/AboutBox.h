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
// "About" overlay shown over the panel: logo, name, version, date, author,
// licence and a short description. Click anywhere, the close button or
// press Escape to close.

#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::ui
{

/** Three-bar "burger" menu button. */
class BurgerButton : public juce::Button
{
public:
    BurgerButton() : juce::Button ("Menu") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Square button with a cross, to close an overlay. */
class CloseButton : public juce::Button
{
public:
    CloseButton() : juce::Button ("Close") { setTooltip ("Close (Esc)"); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class AboutBox : public juce::Component
{
public:
    explicit AboutBox (const juce::Image& woodTexture);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void show();
    void hide();

private:
    juce::Rectangle<float> cardBounds() const;
    const juce::Image& wood;
    CloseButton closeButton;
};

} // namespace mm::ui
