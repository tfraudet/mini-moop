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
// "Sound Banks" overlay shown over the panel (opened from the burger menu):
// list of banks on the left, content of the selected bank by category on the
// right. Activate / create / duplicate / rename / delete / import / export
// banks, load / rename / delete presets, save the current sound. The preset
// list can be filtered by category (click: show / hide, double-click: solo).

#include "../SoundBank.h"
#include "AboutBox.h" // CloseButton

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <set>

class MiniMoopProcessor;

namespace mm::ui
{

/** Small card drawn over its parent: title, message, text fields (optionally
    with a menu of suggestions) and buttons. Stays inside the plugin window. */
class Prompt : public juce::Component
{
public:
    struct Field
    {
        juce::String label, text;
        juce::StringArray suggestions;
    };
    /** `button` is the index in the button list; Return picks the first, Escape the last. */
    using Callback = std::function<void (int button, const juce::StringArray& values)>;

    Prompt();

    void ask (const juce::String& title, const juce::String& message, std::vector<Field> fields,
              const juce::StringArray& buttonNames, Callback onDone);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override {} // modal: the clicks stop here

private:
    void finish (int button);
    juce::Rectangle<int> card() const;
    int messageHeight() const;

    juce::String title, message;
    std::vector<Field> specs;
    std::vector<std::unique_ptr<juce::TextEditor>> editors;
    std::vector<std::unique_ptr<juce::TextButton>> suggest, buttons;
    Callback callback;
};

class BankManager : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit BankManager (MiniMoopProcessor&);
    ~BankManager() override;

    void show();
    void hide();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct BankRows : juce::ListBoxModel
    {
        explicit BankRows (BankManager& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int lastRow) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void returnKeyPressed (int row) override;
        void deleteKeyPressed (int row) override;
        BankManager& owner;
    };

    struct PresetRows : juce::ListBoxModel
    {
        explicit PresetRows (BankManager& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int lastRow) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void returnKeyPressed (int row) override;
        void deleteKeyPressed (int row) override;
        BankManager& owner;
    };

    struct CategoryRows : juce::ListBoxModel
    {
        explicit CategoryRows (BankManager& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void returnKeyPressed (int row) override;
        BankManager& owner;
    };

    /** One line of the preset list: a category heading (preset < 0) or a preset. */
    struct Row { int category = -1, preset = -1, flat = -1; };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refreshBanks();
    void refreshPresets();
    void updateButtons();
    void selectBank (const juce::String& id);
    void selectPreset (int flatIndex);
    std::shared_ptr<const SoundBank> selectedBank() const;
    int selectedPreset() const;
    bool isActive (const juce::String& bankId) const;

    // category filter (per bank, for the session)
    bool isHidden (const juce::String& category) const;
    void toggleCategory (int index);
    void soloCategory (int index);
    void showAllCategories (bool show);

    // actions
    void activateBank();
    void newBank();
    void duplicateBank();
    void renameBank();
    void deleteBank();
    void importBanks();
    void exportBank();
    void revealBank();
    void loadPreset();
    void renamePreset();
    void deletePreset();
    void saveCurrentSound();
    void askSave (const juce::String& bankId);
    void doSave (const juce::String& bankId, const juce::String& category, const juce::String& name);
    void showError (const juce::String& title, const juce::Result&);

    juce::Rectangle<float> cardBounds() const;

    MiniMoopProcessor& proc;
    BankLibrary& library;

    std::vector<std::shared_ptr<const SoundBank>> banks;
    juce::String selectedId;
    std::shared_ptr<const SoundBank> shownBank;
    std::vector<Row> rows;

    BankRows bankRows { *this };
    PresetRows presetRows { *this };
    CategoryRows categoryRows { *this };
    juce::ListBox bankList { "Banks", &bankRows }, presetList { "Presets", &presetRows },
        categoryList { "Categories", &categoryRows };
    std::map<juce::String, std::set<juce::String>> hiddenCategories; // bank id -> hidden category names

    juce::TextButton activateButton { "Activate" }, newButton { "New" }, duplicateButton { "Duplicate" },
        renameButton { "Rename" }, deleteButton { "Delete" }, importButton { "Import..." },
        exportButton { "Export..." }, revealButton { "Show File" };
    juce::TextButton loadButton { "Load" }, renamePresetButton { "Rename" }, deletePresetButton { "Delete" },
        saveButton { "Save Current Sound..." };
    CloseButton closeButton;
    juce::TextButton allCategoriesButton { "All" }, noCategoriesButton { "None" };

    juce::Rectangle<float> infoArea, banksCaption, categoriesCaption, presetsCaption, footerArea;
    juce::String shownActiveId;
    int shownLoadedProgram = -2;

    std::unique_ptr<juce::FileChooser> chooser;
    Prompt prompt;
};

} // namespace mm::ui
