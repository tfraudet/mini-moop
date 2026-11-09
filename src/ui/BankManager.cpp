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

#include "BankManager.h"

#include "../PluginProcessor.h"
#include "PanelLookAndFeel.h"

namespace mm::ui
{

namespace
{
const juce::Colour listBack { 0xff111113 };
const juce::Colour selectedBack = colours::rockerOrange.withAlpha (0.32f);

void drawCard (juce::Graphics& g, juce::Rectangle<float> card)
{
    juce::Path shape;
    shape.addRoundedRectangle (card, 10.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 24, { 0, 8 }).drawForPath (g, shape);
    juce::ColourGradient face (juce::Colour (0xff26262a), card.getTopLeft(), juce::Colour (0xff161618), card.getBottomLeft(), false);
    g.setGradientFill (face);
    g.fillPath (shape);
    g.setColour (colours::lcdText.withAlpha (0.55f));
    g.strokePath (shape, juce::PathStrokeType (1.2f));
}

void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area)
{
    g.setColour (colours::stripLabel);
    g.setFont (PanelLookAndFeel::legendFont (11.5f));
    g.drawText (text, area, juce::Justification::centredLeft);
}

void styleEditor (juce::TextEditor& e)
{
    e.setFont (PanelLookAndFeel::legendFont (15.0f, false));
    e.setColour (juce::TextEditor::backgroundColourId, listBack);
    e.setColour (juce::TextEditor::textColourId, colours::legend);
    e.setColour (juce::TextEditor::outlineColourId, colours::legendDim.withAlpha (0.4f));
    e.setColour (juce::TextEditor::focusedOutlineColourId, colours::lcdText.withAlpha (0.8f));
    e.setColour (juce::TextEditor::highlightColourId, colours::rockerOrange.withAlpha (0.45f));
    e.setColour (juce::CaretComponent::caretColourId, colours::lcdText);
    e.setIndents (8, 6);
}

void styleList (juce::ListBox& l, int rowHeight)
{
    l.setRowHeight (rowHeight);
    l.setColour (juce::ListBox::backgroundColourId, listBack);
    l.setColour (juce::ListBox::outlineColourId, colours::legendDim.withAlpha (0.3f));
    l.setOutlineThickness (1);
    l.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, colours::legendDim.withAlpha (0.5f));
}

juce::String programNumber (int flat) { return juce::String (flat + 1).paddedLeft ('0', 3); }

juce::String summary (const SoundBank& b)
{
    const int n = b.numPresets(), c = int (b.categories.size());
    return juce::String (n) + (n == 1 ? " preset" : " presets") + juce::String::fromUTF8 ("  \xc2\xb7  ")
         + juce::String (c) + (c == 1 ? " category" : " categories");
}
} // namespace

// ============================================================================ Prompt

Prompt::Prompt()
{
    setWantsKeyboardFocus (true);
    setInterceptsMouseClicks (true, true);
}

void Prompt::ask (const juce::String& t, const juce::String& m, std::vector<Field> fields,
                  const juce::StringArray& buttonNames, Callback onDone)
{
    title = t;
    message = m;
    specs = std::move (fields);
    callback = std::move (onDone);
    editors.clear();
    suggest.clear();
    buttons.clear();

    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto e = std::make_unique<juce::TextEditor>();
        styleEditor (*e);
        e->setText (specs[i].text, false);
        e->onReturnKey = [this] { finish (0); };
        e->onEscapeKey = [this] { finish (int (buttons.size()) - 1); };
        addAndMakeVisible (*e);
        editors.push_back (std::move (e));

        auto s = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xe2\x96\xbe"));
        s->setTooltip ("Choose an existing one");
        s->onClick = [this, i] {
            juce::PopupMenu menu;
            menu.setLookAndFeel (&getLookAndFeel());
            for (const auto& item : specs[i].suggestions)
                menu.addItem (item, [this, i, item] { editors[i]->setText (item, false); editors[i]->grabKeyboardFocus(); });
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (suggest[i].get()));
        };
        addChildComponent (*s);
        s->setVisible (! specs[i].suggestions.isEmpty());
        suggest.push_back (std::move (s));
    }
    for (int i = 0; i < buttonNames.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (buttonNames[i]);
        b->onClick = [this, i] { finish (i); };
        addAndMakeVisible (*b);
        buttons.push_back (std::move (b));
    }

    resized();
    setVisible (true);
    toFront (false);
    if (! editors.empty())
    {
        editors.front()->grabKeyboardFocus();
        editors.front()->selectAll();
    }
    else
        grabKeyboardFocus();
    repaint();
}

void Prompt::finish (int button)
{
    if (! isVisible()) return;
    juce::StringArray values;
    for (auto& e : editors) values.add (e->getText().trim());
    setVisible (false);
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
    // Run the callback once the button / editor that triggered it has returned:
    // it may open another prompt, which rebuilds the children.
    auto cb = std::move (callback);
    callback = nullptr;
    if (cb != nullptr)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Prompt> (this), cb, button, values] {
            if (safe != nullptr) cb (button, values);
        });
}

bool Prompt::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { finish (int (buttons.size()) - 1); return true; }
    if (k == juce::KeyPress::returnKey) { finish (0); return true; }
    return true; // modal: nothing goes through to the bank manager
}

int Prompt::messageHeight() const
{
    if (message.isEmpty()) return 0;
    juce::AttributedString s;
    s.append (message, PanelLookAndFeel::legendFont (14.0f, false), colours::legendDim);
    juce::TextLayout layout;
    layout.createLayout (s, 460.0f - 48.0f);
    return int (std::ceil (layout.getHeight())) + 10;
}

juce::Rectangle<int> Prompt::card() const
{
    const int h = 24 + 30 + messageHeight() + int (specs.size()) * 58 + 12 + 32 + 22;
    return juce::Rectangle<int> (460, h).withCentre (getLocalBounds().getCentre());
}

void Prompt::resized()
{
    auto r = card().reduced (24, 24);
    r.removeFromTop (30 + messageHeight());
    for (size_t i = 0; i < editors.size(); ++i)
    {
        r.removeFromTop (18); // label
        auto line = r.removeFromTop (32);
        if (suggest[i]->isVisible())
        {
            suggest[i]->setBounds (line.removeFromRight (32));
            line.removeFromRight (6);
        }
        editors[i]->setBounds (line);
        r.removeFromTop (8);
    }
    auto buttonRow = card().reduced (24, 22).removeFromBottom (32);
    for (int i = int (buttons.size()) - 1; i >= 0; --i)
    {
        buttons[size_t (i)]->setBounds (buttonRow.removeFromRight (110));
        buttonRow.removeFromRight (10);
    }
}

void Prompt::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.5f));
    const auto c = card().toFloat();
    drawCard (g, c);

    auto r = c.reduced (24.0f, 24.0f);
    g.setColour (colours::legend);
    g.setFont (PanelLookAndFeel::legendFont (19.0f));
    g.drawText (title, r.removeFromTop (30.0f), juce::Justification::centredLeft);
    if (const int mh = messageHeight(); mh > 0)
    {
        g.setColour (colours::legendDim);
        g.setFont (PanelLookAndFeel::legendFont (14.0f, false));
        g.drawFittedText (message, r.removeFromTop (float (mh)).toNearestInt(), juce::Justification::topLeft, 8, 1.0f);
    }
    for (const auto& f : specs)
    {
        caption (g, f.label.toUpperCase(), r.removeFromTop (18.0f));
        r.removeFromTop (40.0f);
    }
}

// ============================================================================ BankManager

BankManager::BankManager (MiniMoopProcessor& p) : proc (p), library (p.getLibrary())
{
    setWantsKeyboardFocus (true);
    setInterceptsMouseClicks (true, true);

    styleList (bankList, 48);
    styleList (presetList, 26);
    styleList (categoryList, 26);
    categoryList.setTooltip ("Click a category to show or hide it, double-click to show only that one");
    addAndMakeVisible (bankList);
    addAndMakeVisible (presetList);
    addAndMakeVisible (categoryList);

    auto button = [this] (juce::TextButton& b, const juce::String& tip, std::function<void()> action) {
        b.setTooltip (tip);
        b.onClick = std::move (action);
        addAndMakeVisible (b);
    };
    button (activateButton, "Use this bank for the PROGRAM display, the host's program list and new instances, "
                            "and load its first program",
            [this] { activateBank(); });
    button (newButton, "Create an empty bank", [this] { newBank(); });
    button (duplicateButton, "Copy this bank into a new one you can edit", [this] { duplicateBank(); });
    button (renameButton, "Rename this bank", [this] { renameBank(); });
    button (deleteButton, "Move this bank's file to the Trash", [this] { deleteBank(); });
    button (importButton, "Add banks from .json files", [this] { importBanks(); });
    button (exportButton, "Save a copy of this bank as a .json file", [this] { exportBank(); });
    button (revealButton, "Show this bank's file in the banks folder", [this] { revealBank(); });
    button (loadButton, "Load the selected preset (double-click or Return also works)", [this] { loadPreset(); });
    button (renamePresetButton, "Rename the selected preset", [this] { renamePreset(); });
    button (deletePresetButton, "Remove the selected preset from the bank", [this] { deletePreset(); });
    button (saveButton, "Store the current panel settings as a preset of the selected bank",
            [this] { saveCurrentSound(); });
    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);
    button (allCategoriesButton, "Show every category", [this] { showAllCategories (true); });
    button (noCategoriesButton, "Hide every category", [this] { showAllCategories (false); });

    addChildComponent (prompt);
    library.addChangeListener (this);
}

BankManager::~BankManager()
{
    library.removeChangeListener (this);
    stopTimer();
}

void BankManager::show()
{
    library.rescan(); // pick up files added to the folder by hand
    refreshBanks();
    selectBank (proc.getActiveBank()->id);
    if (proc.getLoadedProgram() >= 0) selectPreset (proc.getLoadedProgram());
    setVisible (true);
    toFront (true);
    bankList.grabKeyboardFocus();
    startTimerHz (5);
}

void BankManager::hide()
{
    prompt.setVisible (false);
    stopTimer();
    setVisible (false);
}

void BankManager::mouseDown (const juce::MouseEvent& e)
{
    if (! cardBounds().contains (e.position)) hide();
}

bool BankManager::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }
    return false;
}

void BankManager::changeListenerCallback (juce::ChangeBroadcaster*) { refreshBanks(); }

void BankManager::timerCallback()
{
    // follow program / bank changes made from the panel or the host
    const auto activeId = proc.getActiveBank()->id;
    const int loaded = proc.getLoadedProgram();
    if (activeId != shownActiveId || loaded != shownLoadedProgram)
    {
        shownActiveId = activeId;
        shownLoadedProgram = loaded;
        bankList.repaint();
        presetList.repaint();
        updateButtons();
    }
}

// ---------------------------------------------------------------------------- model

std::shared_ptr<const SoundBank> BankManager::selectedBank() const { return shownBank; }

bool BankManager::isActive (const juce::String& bankId) const { return proc.getActiveBank()->id == bankId; }

int BankManager::selectedPreset() const
{
    const int row = presetList.getSelectedRow();
    return juce::isPositiveAndBelow (row, int (rows.size())) ? rows[size_t (row)].flat : -1;
}

void BankManager::refreshBanks()
{
    banks = library.getBanks();
    bankList.updateContent();
    auto it = std::find_if (banks.begin(), banks.end(), [&] (const auto& b) { return b->id == selectedId; });
    if (it == banks.end())
    {
        selectedId = proc.getActiveBank()->id;
        it = std::find_if (banks.begin(), banks.end(), [&] (const auto& b) { return b->id == selectedId; });
        if (it == banks.end()) { selectedId = BankLibrary::factoryId; it = banks.begin(); }
    }
    bankList.selectRow (int (it - banks.begin()), true, true);
    refreshPresets();
    bankList.repaint();
}

void BankManager::refreshPresets()
{
    const int keep = selectedPreset();
    const auto previous = shownBank;
    shownBank = library.getBank (selectedId);
    rows.clear();
    if (shownBank != nullptr)
        for (int c = 0; c < int (shownBank->categories.size()); ++c)
        {
            if (isHidden (shownBank->categories[size_t (c)].name))
                continue;
            rows.push_back ({ c, -1, -1 });
            for (int p = 0; p < int (shownBank->categories[size_t (c)].presets.size()); ++p)
                rows.push_back ({ c, p, shownBank->flatIndexOf (c, p) });
        }
    presetList.updateContent();
    categoryList.updateContent();
    categoryList.repaint();
    if (previous == nullptr || shownBank == nullptr || previous->id != shownBank->id)
    {
        presetList.deselectAllRows();
        presetList.scrollToEnsureRowIsOnscreen (0);
    }
    else
        selectPreset (keep);
    presetList.repaint();
    updateButtons();
    repaint();
}

void BankManager::selectBank (const juce::String& id)
{
    selectedId = id;
    refreshBanks();
}

void BankManager::selectPreset (int flatIndex)
{
    for (int r = 0; r < int (rows.size()); ++r)
        if (rows[size_t (r)].flat == flatIndex && flatIndex >= 0)
        {
            presetList.selectRow (r);
            return;
        }
    presetList.deselectAllRows();
}

void BankManager::updateButtons()
{
    const auto bank = selectedBank();
    const bool have = bank != nullptr, writable = have && ! bank->readOnly;
    const bool preset = have && selectedPreset() >= 0;
    activateButton.setEnabled (have && ! isActive (bank->id));
    duplicateButton.setEnabled (have);
    renameButton.setEnabled (writable);
    deleteButton.setEnabled (writable);
    exportButton.setEnabled (have);
    revealButton.setEnabled (have);
    loadButton.setEnabled (preset);
    const int numCategories = have ? int (bank->categories.size()) : 0;
    const auto hidden = have ? hiddenCategories.find (bank->id) : hiddenCategories.end();
    const int numHidden = hidden != hiddenCategories.end() ? int (hidden->second.size()) : 0;
    allCategoriesButton.setEnabled (numHidden > 0);
    noCategoriesButton.setEnabled (numCategories > 0 && numHidden < numCategories);
    renamePresetButton.setEnabled (preset && writable);
    deletePresetButton.setEnabled (preset && writable);
}

// ---------------------------------------------------------------------------- bank list

int BankManager::BankRows::getNumRows() { return int (owner.banks.size()); }

void BankManager::BankRows::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, int (owner.banks.size()))) return;
    const auto& b = *owner.banks[size_t (row)];
    auto r = juce::Rectangle<float> (float (w), float (h));
    if (selected)
    {
        g.setColour (selectedBack);
        g.fillRect (r);
    }
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (h - 1, 0.0f, float (w));

    // active bank: lit amber LED; others: dark LED
    const bool active = owner.isActive (b.id);
    const auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ 16.0f, float (h) * 0.5f });
    if (active)
    {
        g.setColour (colours::lcdText.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (active ? colours::lcdText : juce::Colour (0xff2c2a26));
    g.fillEllipse (led);

    r.removeFromLeft (32.0f);
    r.removeFromRight (8.0f);
    auto top = r.removeFromTop (float (h) * 0.55f);
    if (b.readOnly)
    {
        g.setColour (colours::stripLabel.withAlpha (0.85f));
        g.setFont (PanelLookAndFeel::legendFont (10.0f));
        g.drawText ("READ-ONLY", top.removeFromRight (64.0f).withTrimmedTop (6.0f), juce::Justification::centredRight);
    }
    g.setColour (active ? colours::lcdText : colours::legend);
    g.setFont (PanelLookAndFeel::legendFont (15.0f));
    g.drawText (b.name, top.withTrimmedTop (6.0f), juce::Justification::centredLeft, true);
    g.setColour (colours::legendDim);
    g.setFont (PanelLookAndFeel::legendFont (11.5f, false));
    g.drawText (summary (b), r.withTrimmedBottom (6.0f), juce::Justification::centredLeft, true);
}

void BankManager::BankRows::selectedRowsChanged (int row)
{
    if (! juce::isPositiveAndBelow (row, int (owner.banks.size()))) return;
    const auto id = owner.banks[size_t (row)]->id;
    if (id == owner.selectedId) return;
    owner.selectedId = id;
    owner.refreshPresets();
}

void BankManager::BankRows::listBoxItemDoubleClicked (int, const juce::MouseEvent&) { owner.activateBank(); }
void BankManager::BankRows::returnKeyPressed (int) { owner.activateBank(); }
void BankManager::BankRows::deleteKeyPressed (int) { owner.deleteBank(); }

// ---------------------------------------------------------------------------- preset list

int BankManager::PresetRows::getNumRows() { return int (owner.rows.size()); }

void BankManager::PresetRows::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, int (owner.rows.size())) || owner.shownBank == nullptr) return;
    const auto& line = owner.rows[size_t (row)];
    const auto& category = owner.shownBank->categories[size_t (line.category)];
    auto r = juce::Rectangle<float> (float (w), float (h)).reduced (10.0f, 0.0f);

    if (line.preset < 0)
    {
        g.setColour (colours::stripLabel);
        g.setFont (PanelLookAndFeel::legendFont (11.5f));
        const auto text = category.name.toUpperCase();
        g.drawText (text, r.withTrimmedTop (6.0f), juce::Justification::centredLeft, true);
        const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
        g.setColour (colours::stripLabel.withAlpha (0.35f));
        g.drawHorizontalLine (int (r.getCentreY() + 3.0f), r.getX() + textW + 10.0f, r.getRight());
        return;
    }

    if (selected)
    {
        g.setColour (selectedBack);
        g.fillRect (0, 0, w, h);
    }
    const bool loaded = owner.isActive (owner.shownBank->id) && owner.proc.getLoadedProgram() == line.flat;
    r.removeFromLeft (8.0f);
    g.setFont (PanelLookAndFeel::lcdFont (13.0f));
    g.setColour (loaded ? colours::lcdText : colours::legendDim.withAlpha (0.7f));
    g.drawText (programNumber (line.flat), r.removeFromLeft (40.0f), juce::Justification::centredLeft);
    g.setFont (PanelLookAndFeel::legendFont (14.5f, false));
    g.setColour (loaded ? colours::lcdText : colours::legend);
    const auto& name = category.presets[size_t (line.preset)].name;
    g.drawText (name, r, juce::Justification::centredLeft, true);
    if (loaded)
    {
        g.setFont (PanelLookAndFeel::legendFont (10.0f));
        g.drawText ("LOADED", r, juce::Justification::centredRight);
    }
}

void BankManager::PresetRows::selectedRowsChanged (int row)
{
    // headings are not selectable: move to the first preset of the category
    if (juce::isPositiveAndBelow (row, int (owner.rows.size())) && owner.rows[size_t (row)].preset < 0)
    {
        if (row + 1 < int (owner.rows.size()) && owner.rows[size_t (row + 1)].preset >= 0)
            owner.presetList.selectRow (row + 1);
        else
            owner.presetList.deselectAllRows();
        return;
    }
    owner.updateButtons();
}

void BankManager::PresetRows::listBoxItemDoubleClicked (int, const juce::MouseEvent&) { owner.loadPreset(); }
void BankManager::PresetRows::returnKeyPressed (int) { owner.loadPreset(); }
void BankManager::PresetRows::deleteKeyPressed (int) { owner.deletePreset(); }

// ---------------------------------------------------------------------------- category filter

bool BankManager::isHidden (const juce::String& category) const
{
    if (shownBank == nullptr) return false;
    const auto it = hiddenCategories.find (shownBank->id);
    return it != hiddenCategories.end() && it->second.count (category) > 0;
}

void BankManager::toggleCategory (int index)
{
    if (shownBank == nullptr || ! juce::isPositiveAndBelow (index, int (shownBank->categories.size()))) return;
    auto& hidden = hiddenCategories[shownBank->id];
    const auto& name = shownBank->categories[size_t (index)].name;
    if (! hidden.erase (name)) hidden.insert (name);
    refreshPresets();
}

void BankManager::soloCategory (int index)
{
    if (shownBank == nullptr || ! juce::isPositiveAndBelow (index, int (shownBank->categories.size()))) return;
    auto& hidden = hiddenCategories[shownBank->id];
    hidden.clear();
    for (int c = 0; c < int (shownBank->categories.size()); ++c)
        if (c != index) hidden.insert (shownBank->categories[size_t (c)].name);
    refreshPresets();
}

void BankManager::showAllCategories (bool show)
{
    if (shownBank == nullptr) return;
    auto& hidden = hiddenCategories[shownBank->id];
    hidden.clear();
    if (! show)
        for (const auto& c : shownBank->categories) hidden.insert (c.name);
    refreshPresets();
}

int BankManager::CategoryRows::getNumRows()
{
    return owner.shownBank != nullptr ? int (owner.shownBank->categories.size()) : 0;
}

void BankManager::CategoryRows::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (owner.shownBank == nullptr || ! juce::isPositiveAndBelow (row, getNumRows())) return;
    const auto& category = owner.shownBank->categories[size_t (row)];
    const bool shown = ! owner.isHidden (category.name);
    auto r = juce::Rectangle<float> (float (w), float (h)).reduced (10.0f, 0.0f);

    // check box: lit amber when the category is shown
    const auto box = r.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 12.0f);
    g.setColour (shown ? colours::lcdText : juce::Colour (0xff2c2a26));
    g.fillRoundedRectangle (box, 2.5f);
    if (shown)
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 2.5f, box.getCentreY());
        tick.lineTo (box.getX() + 5.0f, box.getBottom() - 3.0f);
        tick.lineTo (box.getRight() - 2.5f, box.getY() + 3.0f);
        g.setColour (listBack);
        g.strokePath (tick, juce::PathStrokeType (1.8f));
    }
    r.removeFromLeft (8.0f);

    g.setFont (PanelLookAndFeel::lcdFont (12.0f));
    g.setColour (colours::legendDim.withAlpha (shown ? 0.7f : 0.35f));
    g.drawText (juce::String (category.presets.size()), r.removeFromRight (36.0f), juce::Justification::centredRight);
    g.setFont (PanelLookAndFeel::legendFont (14.0f, false));
    g.setColour (shown ? colours::legend : colours::legendDim.withAlpha (0.5f));
    g.drawText (category.name, r, juce::Justification::centredLeft, true);
}

void BankManager::CategoryRows::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() < 2) owner.toggleCategory (row);
}

void BankManager::CategoryRows::listBoxItemDoubleClicked (int row, const juce::MouseEvent&) { owner.soloCategory (row); }
void BankManager::CategoryRows::returnKeyPressed (int row) { owner.toggleCategory (row); }

// ---------------------------------------------------------------------------- bank actions

void BankManager::showError (const juce::String& title, const juce::Result& r)
{
    prompt.ask (title, r.getErrorMessage(), {}, { "OK" }, nullptr);
}

void BankManager::activateBank()
{
    // Like switching banks on a hardware synth: the first program is loaded.
    const auto bank = selectedBank();
    if (bank == nullptr || isActive (bank->id)) return; // never reset the sound of the active bank
    proc.loadPreset (bank->id, 0);
    if (const int loaded = proc.getLoadedProgram(); loaded >= 0)
        selectPreset (loaded);
    timerCallback();
}

void BankManager::newBank()
{
    prompt.ask ("New Bank", "Create an empty bank, then save sounds into it with \"Save Current Sound...\".",
                { { "Name", "My Bank", {} } }, { "Create", "Cancel" },
                [this] (int button, const juce::StringArray& v) {
                    if (button != 0) return;
                    juce::String id;
                    if (const auto r = library.createBank (v[0], id); r.failed())
                        return showError ("New Bank", r);
                    selectBank (id);
                });
}

void BankManager::duplicateBank()
{
    const auto bank = selectedBank();
    if (bank == nullptr) return;
    juce::String id;
    if (const auto r = library.duplicateBank (bank->id, id); r.failed())
        return showError ("Duplicate Bank", r);
    selectBank (id);
}

void BankManager::renameBank()
{
    const auto bank = selectedBank();
    if (bank == nullptr || bank->readOnly) return;
    const auto oldId = bank->id;
    prompt.ask ("Rename Bank", {}, { { "Name", bank->name, {} } }, { "Rename", "Cancel" },
                [this, oldId] (int button, const juce::StringArray& v) {
                    if (button != 0) return;
                    juce::String id;
                    if (const auto r = library.renameBank (oldId, v[0], id); r.failed())
                        return showError ("Rename Bank", r);
                    if (id != oldId && isActive (oldId))
                    {
                        const int loaded = proc.getLoadedProgram();
                        proc.setActiveBank (id, false);
                        proc.reloadActiveBank (loaded);
                    }
                    selectBank (id);
                });
}

void BankManager::deleteBank()
{
    const auto bank = selectedBank();
    if (bank == nullptr || bank->readOnly) return;
    const auto id = bank->id;
    prompt.ask ("Delete Bank",
                "Delete the bank \"" + bank->name + "\" and its " + juce::String (bank->numPresets())
                    + " presets? Its file is moved to the Trash.",
                {}, { "Delete", "Cancel" },
                [this, id] (int button, const juce::StringArray&) {
                    if (button != 0) return;
                    if (const auto r = library.deleteBank (id); r.failed())
                        return showError ("Delete Bank", r);
                    if (isActive (id)) proc.setActiveBank (BankLibrary::factoryId, false);
                    selectBank (proc.getActiveBank()->id);
                });
}

void BankManager::importBanks()
{
    chooser = std::make_unique<juce::FileChooser> ("Import Sound Banks",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe = juce::Component::SafePointer<BankManager> (this)] (const juce::FileChooser& fc) {
                              if (safe == nullptr) return;
                              juce::StringArray errors;
                              juce::String last;
                              for (const auto& f : fc.getResults())
                              {
                                  juce::String id;
                                  if (const auto r = safe->library.importBank (f, id); r.failed())
                                      errors.add (r.getErrorMessage());
                                  else
                                      last = id;
                              }
                              if (last.isNotEmpty()) safe->selectBank (last);
                              if (! errors.isEmpty())
                                  safe->showError ("Import", juce::Result::fail (errors.joinIntoString ("\n")));
                          });
}

void BankManager::exportBank()
{
    const auto bank = selectedBank();
    if (bank == nullptr) return;
    const auto id = bank->id;
    chooser = std::make_unique<juce::FileChooser> (
        "Export Sound Bank",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (id + ".json"), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<BankManager> (this), id] (const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File()) return;
                              auto file = fc.getResult();
                              if (! file.hasFileExtension ("json")) file = file.withFileExtension ("json");
                              if (const auto r = safe->library.exportBank (id, file); r.failed())
                                  safe->showError ("Export", r);
                          });
}

void BankManager::revealBank()
{
    if (const auto bank = selectedBank(); bank != nullptr && bank->file.existsAsFile())
        bank->file.revealToUser();
    else
        library.getDirectory().revealToUser();
}

// ---------------------------------------------------------------------------- preset actions

void BankManager::loadPreset()
{
    const auto bank = selectedBank();
    const int flat = selectedPreset();
    if (bank == nullptr || flat < 0) return;
    proc.loadPreset (bank->id, flat);
    timerCallback();
}

void BankManager::renamePreset()
{
    const auto bank = selectedBank();
    const int flat = selectedPreset();
    if (bank == nullptr || bank->readOnly || flat < 0) return;
    const auto id = bank->id;
    prompt.ask ("Rename Preset", {}, { { "Name", bank->presetAt (flat)->name, {} } }, { "Rename", "Cancel" },
                [this, id, flat] (int button, const juce::StringArray& v) {
                    if (button != 0) return;
                    if (const auto r = library.renamePreset (id, flat, v[0]); r.failed())
                        return showError ("Rename Preset", r);
                    if (isActive (id)) proc.reloadActiveBank (proc.getLoadedProgram());
                    refreshBanks();
                    selectPreset (flat);
                });
}

void BankManager::deletePreset()
{
    const auto bank = selectedBank();
    const int flat = selectedPreset();
    if (bank == nullptr || bank->readOnly || flat < 0) return;
    const auto id = bank->id;
    prompt.ask ("Delete Preset", "Delete \"" + bank->presetAt (flat)->name + "\" from the bank \"" + bank->name + "\"?",
                {}, { "Delete", "Cancel" },
                [this, id, flat] (int button, const juce::StringArray&) {
                    if (button != 0) return;
                    if (const auto r = library.deletePreset (id, flat); r.failed())
                        return showError ("Delete Preset", r);
                    if (isActive (id))
                    {
                        // program numbers after the deleted one move down by one
                        const int loaded = proc.getLoadedProgram();
                        proc.reloadActiveBank (loaded == flat ? -1 : loaded > flat ? loaded - 1 : loaded);
                    }
                    refreshBanks();
                    selectPreset (flat);
                });
}

void BankManager::saveCurrentSound()
{
    const auto bank = selectedBank();
    if (bank != nullptr && ! bank->readOnly)
        return askSave (bank->id);

    prompt.ask ("Save Current Sound",
                "The " + (bank != nullptr ? bank->name : juce::String ("Factory"))
                    + " bank is read-only. Create a new bank to save your sound in:",
                { { "Bank name", "My Bank", {} } }, { "Create", "Cancel" },
                [this] (int button, const juce::StringArray& v) {
                    if (button != 0) return;
                    juce::String id;
                    if (const auto r = library.createBank (v[0], id); r.failed())
                        return showError ("New Bank", r);
                    selectBank (id);
                    askSave (id);
                });
}

void BankManager::askSave (const juce::String& bankId)
{
    const auto bank = library.getBank (bankId);
    if (bank == nullptr) return;

    // Suggest the name of the loaded program and the category of the selected preset.
    juce::String name = "New Sound", category = "User";
    const auto active = proc.getActiveBank();
    if (const auto* loaded = active->presetAt (proc.getLoadedProgram()))
    {
        name = loaded->name;
        if (active->id == bankId) category = active->categoryOf (proc.getLoadedProgram())->name;
    }
    if (shownBank != nullptr && shownBank->id == bankId)
        if (const auto* c = shownBank->categoryOf (selectedPreset()))
            category = c->name;
    juce::StringArray categories;
    for (const auto& c : bank->categories) categories.add (c.name);
    if (categories.isEmpty()) categories.add ("User");

    prompt.ask ("Save Current Sound", "Store the current panel settings in the bank \"" + bank->name + "\".",
                { { "Preset name", name, {} }, { "Category", category, categories } }, { "Save", "Cancel" },
                [this, bankId] (int button, const juce::StringArray& v) {
                    if (button != 0) return;
                    const auto target = library.getBank (bankId);
                    if (target != nullptr && target->find (v[1], v[0]) >= 0)
                        return prompt.ask ("Replace Preset",
                                           "\"" + v[0] + "\" already exists in " + v[1] + ". Replace it with the current sound?",
                                           {}, { "Replace", "Cancel" },
                                           [this, bankId, v] (int b, const juce::StringArray&) {
                                               if (b == 0) doSave (bankId, v[1], v[0]);
                                           });
                    doSave (bankId, v[1], v[0]);
                });
}

void BankManager::doSave (const juce::String& bankId, const juce::String& category, const juce::String& name)
{
    int flat = -1;
    if (const auto r = library.savePreset (bankId, category, name, proc.captureCurrentValues(), flat); r.failed())
        return showError ("Save Current Sound", r);
    // The sound now is that program: make its bank the active one.
    proc.setActiveBank (bankId);
    proc.reloadActiveBank (flat);
    selectBank (bankId);
    selectPreset (flat);
    timerCallback();
}

// ---------------------------------------------------------------------------- layout

juce::Rectangle<float> BankManager::cardBounds() const
{
    return juce::Rectangle<float> (1120.0f, 640.0f).withCentre (getLocalBounds().toFloat().getCentre());
}

void BankManager::resized()
{
    prompt.setBounds (getLocalBounds());

    auto r = cardBounds().reduced (26.0f, 22.0f).toNearestInt();
    auto titleRow = r.removeFromTop (40);
    closeButton.setBounds (titleRow.removeFromRight (30).withSizeKeepingCentre (30, 30));
    r.removeFromTop (8);
    footerArea = r.removeFromBottom (22).toFloat();
    r.removeFromBottom (10);

    // left: banks
    auto left = r.removeFromLeft (340);
    r.removeFromLeft (26);
    banksCaption = left.removeFromTop (20).toFloat();
    auto buttonsArea = left.removeFromBottom (4 * 30 + 3 * 8);
    left.removeFromBottom (12);
    bankList.setBounds (left);
    juce::TextButton* grid[] = { &activateButton, &newButton, &duplicateButton, &renameButton,
                                 &importButton, &exportButton, &deleteButton, &revealButton };
    const int bw = (buttonsArea.getWidth() - 8) / 2;
    for (int i = 0; i < 8; ++i)
        grid[i]->setBounds (buttonsArea.getX() + (i % 2) * (bw + 8), buttonsArea.getY() + (i / 2) * 38, bw, 30);

    // right: content of the selected bank, category filter beside the presets
    infoArea = r.removeFromTop (74).toFloat();
    r.removeFromTop (6);
    auto captions = r.removeFromTop (20);
    auto bottom = r.removeFromBottom (30);
    r.removeFromBottom (12);
    constexpr int categoryWidth = 190, gap = 12;
    categoriesCaption = captions.removeFromLeft (categoryWidth).toFloat();
    captions.removeFromLeft (gap);
    presetsCaption = captions.toFloat();
    categoryList.setBounds (r.removeFromLeft (categoryWidth));
    r.removeFromLeft (gap);
    presetList.setBounds (r);

    auto categoryButtons = bottom.removeFromLeft (categoryWidth);
    allCategoriesButton.setBounds (categoryButtons.removeFromLeft ((categoryWidth - 8) / 2));
    noCategoriesButton.setBounds (categoryButtons.removeFromRight ((categoryWidth - 8) / 2));
    bottom.removeFromLeft (gap);
    loadButton.setBounds (bottom.removeFromLeft (86));
    bottom.removeFromLeft (8);
    renamePresetButton.setBounds (bottom.removeFromLeft (86));
    bottom.removeFromLeft (8);
    deletePresetButton.setBounds (bottom.removeFromLeft (86));
    saveButton.setBounds (bottom.removeFromRight (200));
}

void BankManager::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.65f));
    const auto card = cardBounds();
    drawCard (g, card);

    auto title = card.reduced (26.0f, 22.0f).removeFromTop (40.0f);
    g.setColour (colours::legend);
    g.setFont (PanelLookAndFeel::legendFont (24.0f));
    g.drawText ("SOUND BANKS", title, juce::Justification::centredLeft);

    caption (g, "BANKS", banksCaption);
    caption (g, "CATEGORIES", categoriesCaption);
    {
        juce::String presetsTitle = "PRESETS";
        if (const auto bank = selectedBank())
        {
            int visible = 0;
            for (const auto& row : rows) visible += row.preset >= 0 ? 1 : 0;
            if (visible != bank->numPresets())
                presetsTitle << "  (" << visible << " OF " << bank->numPresets() << ")";
        }
        caption (g, presetsTitle, presetsCaption);
    }

    if (const auto bank = selectedBank())
    {
        auto r = infoArea;
        auto nameRow = r.removeFromTop (30.0f);
        if (isActive (bank->id))
        {
            g.setColour (colours::lcdText);
            g.setFont (PanelLookAndFeel::legendFont (11.0f));
            g.drawText ("ACTIVE", nameRow.removeFromRight (60.0f), juce::Justification::centredRight);
        }
        if (bank->readOnly)
        {
            g.setColour (colours::stripLabel);
            g.setFont (PanelLookAndFeel::legendFont (11.0f));
            g.drawText ("READ-ONLY", nameRow.removeFromRight (80.0f), juce::Justification::centredRight);
        }
        g.setColour (colours::legend);
        g.setFont (PanelLookAndFeel::legendFont (21.0f));
        g.drawText (bank->name, nameRow, juce::Justification::centredLeft, true);

        juce::String line = summary (*bank);
        if (bank->author.isNotEmpty()) line = "by " + bank->author + juce::String::fromUTF8 ("  \xc2\xb7  ") + line;
        g.setColour (colours::legendDim);
        g.setFont (PanelLookAndFeel::legendFont (13.0f, false));
        g.drawText (line, r.removeFromTop (20.0f), juce::Justification::centredLeft, true);
        if (bank->description.isNotEmpty())
            g.drawText (bank->description, r.removeFromTop (20.0f), juce::Justification::centredLeft, true);
    }

    g.setFont (PanelLookAndFeel::legendFont (11.5f, false));
    if (const auto errors = library.getScanErrors(); ! errors.isEmpty())
    {
        g.setColour (colours::rockerOrange);
        g.drawText ("Unreadable file(s) in the banks folder: " + errors.joinIntoString ("; "),
                    footerArea.withTrimmedRight (420.0f), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (colours::legendDim.withAlpha (0.65f));
        g.drawText ("Double-click a preset to load it, a bank to activate it, a category to show only that one.  Esc to close.", footerArea,
                    juce::Justification::centredLeft);
    }
    g.setColour (colours::legendDim.withAlpha (0.65f));
    g.drawText (library.getDirectory().getFullPathName(), footerArea.withTrimmedLeft (footerArea.getWidth() - 410.0f),
                juce::Justification::centredRight, true);
}

void BankManager::paintOverChildren (juce::Graphics& g)
{
    // drawn over the (opaque) preset list when it has nothing to show
    const auto bank = selectedBank();
    if (bank == nullptr || prompt.isVisible()) return;
    juce::String text;
    if (bank->numPresets() == 0)
        text = "This bank is empty. Set up a sound on the panel, then click \"Save Current Sound...\".";
    else if (rows.empty())
        text = "No category selected. Tick categories on the left, or click \"All\".";
    if (text.isEmpty()) return;
    g.setColour (colours::legendDim.withAlpha (0.8f));
    g.setFont (PanelLookAndFeel::legendFont (14.0f, false));
    g.drawFittedText (text, presetList.getBounds().reduced (20), juce::Justification::centred, 3);
}

} // namespace mm::ui
