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

// Renders the plugin editor to a PNG (UI verification without a host).
// Usage: MiniMoopSnapshot <out.png> [scale] [about|banks|modern|classic]...
// (a theme argument overrides the saved theme for this render only)
//        MiniMoopSnapshot <out.png> [scale] icon
// renders the application icon instead: the logo on a transparent
// 1024 x 1024 canvas (times `scale`), sized like a macOS app icon.

#include "../../src/PluginEditor.h"
#include "../../src/PluginProcessor.h"
#include "../../src/ui/Logo.h"

#include "MiniMoopAssets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
bool writePng (const juce::Image& img, const juce::File& out)
{
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (img, stream);
    std::printf ("%s %s\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8());
    return ok;
}

/** macOS icon grid: the rounded tile is about 824 of the 1024 canvas, centred,
    the margin left for its shadow. drawLogo insets its tile by 3 %. */
juce::Image renderIcon (int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    const auto wood = juce::ImageCache::getFromMemory (MiniMoopAssets::wood_jpg, MiniMoopAssets::wood_jpgSize);
    const float logo = float (size) * (824.0f / 1024.0f) / 0.94f;
    mm::ui::drawLogo (g, juce::Rectangle<float> (logo, logo).withCentre ({ float (size) * 0.5f, float (size) * 0.5f }), wood);
    return img;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "panel.png"));
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    for (int i = 3; i < argc; ++i)
        if (juce::String (argv[i]) == "icon")
            return writePng (renderIcon (juce::roundToInt (1024.0f * scale)), out) ? 0 : 1;

    MiniMoopProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (mm::ui::Panel::baseWidth, mm::ui::Panel::baseHeight);
    if (auto* e = dynamic_cast<MiniMoopEditor*> (editor.get()))
        for (int i = 3; i < argc; ++i)
        {
            const juce::String arg (argv[i]);
            if (arg == "about") e->showAbout();
            if (arg == "banks") e->showBanks();
            if (arg == "modern" || arg == "classic") e->applyTheme (mm::ui::themeFromId (arg));
        }
    const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    editor.reset();
    return writePng (img, out) ? 0 : 1;
}
