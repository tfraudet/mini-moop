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
// Sound banks: presets grouped by category, one JSON file per bank.
//
// {
//   "format": "MiniMoopBank", "version": 1,
//   "name": "Factory", "author": "...", "description": "...",
//   "categories": [ { "name": "Bass", "presets": [ { "name": "Fat Bass", "values": { "cutoff": 3.5, ... } } ] } ]
// }
//
// Values are in parameter units (pots 0..10, choices by index, switches 0/1),
// keyed by parameter ID. A parameter that is not listed takes its default value.
// Programs are numbered flat, in category order.
//
// The Factory bank is built into the plugin (read-only) and mirrored to the
// banks folder so it can be seen and exported. User banks live in the same folder.

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <memory>
#include <vector>

namespace mm
{

using PresetValues = std::vector<std::pair<juce::String, float>>;

struct BankPreset
{
    juce::String name;
    PresetValues values;
};

struct BankCategory
{
    juce::String name;
    std::vector<BankPreset> presets;
};

struct SoundBank
{
    static constexpr const char* formatTag = "MiniMoopBank";
    static constexpr int formatVersion = 1;

    juce::String id;          // file name without extension
    juce::String name, author, description;
    std::vector<BankCategory> categories;
    bool readOnly = false;
    juce::File file;

    /** Parses a bank. On failure `out` is left untouched and the result holds a readable message. */
    static juce::Result fromJson (const juce::String& json, SoundBank& out);
    juce::String toJson() const;

    struct Location { int category = -1, preset = -1; };

    int numPresets() const noexcept;
    Location locate (int flatIndex) const noexcept;
    const BankPreset* presetAt (int flatIndex) const noexcept;
    const BankCategory* categoryOf (int flatIndex) const noexcept;
    int flatIndexOf (int category, int preset) const noexcept;
    /** Flat index of the preset `name` in category `category`, or -1. */
    int find (const juce::String& category, const juce::String& name) const;
};

/** All the banks on disk. Shared by every plugin instance of the process
    (juce::SharedResourcePointer); used on the message thread. Broadcasts a
    change message whenever a bank is added, modified or removed. */
class BankLibrary : public juce::ChangeBroadcaster
{
public:
    static constexpr const char* factoryId = "Factory";

    /** Banks in the user folder (~/Library/Application Support/Zazart/Mini Moop/presets
        on macOS), Factory from the plugin's binary data. */
    BankLibrary();
    /** Banks in `directory`, settings next to it (used by the tests). */
    BankLibrary (const juce::File& directory, const juce::String& factoryJson);

    static juce::File defaultDirectory();

    juce::File getDirectory() const { return directory; }

    /** Reloads every bank from disk. */
    void rescan();

    /** Factory first, then the user banks by name. */
    std::vector<std::shared_ptr<const SoundBank>> getBanks() const;
    std::shared_ptr<const SoundBank> getBank (const juce::String& id) const;
    std::shared_ptr<const SoundBank> getFactory() const { return getBank (factoryId); }
    /** Files of the banks folder that could not be read, with the reason. */
    juce::StringArray getScanErrors() const;

    juce::Result importBank (const juce::File& source, juce::String& newId);
    juce::Result exportBank (const juce::String& id, const juce::File& destination) const;
    juce::Result createBank (const juce::String& name, juce::String& newId);
    juce::Result duplicateBank (const juce::String& id, juce::String& newId);
    juce::Result renameBank (const juce::String& id, const juce::String& newName, juce::String& newId);
    juce::Result deleteBank (const juce::String& id);

    /** Adds the preset, or replaces the preset with the same name in the same
        category. `flatIndex` receives its program number in the bank. */
    juce::Result savePreset (const juce::String& bankId, const juce::String& category, const juce::String& name,
                             const PresetValues& values, int& flatIndex);
    juce::Result renamePreset (const juce::String& bankId, int flatIndex, const juce::String& newName);
    /** Removes the preset, and its category if it becomes empty. */
    juce::Result deletePreset (const juce::String& bankId, int flatIndex);

    /** Bank used by new plugin instances (the last one activated). */
    juce::String getDefaultBankId() const;
    void setDefaultBankId (const juce::String& id);

    /** File name (without extension) not used by any bank, derived from `name`. */
    juce::String uniqueId (const juce::String& name) const;

private:
    void installFactory (const juce::String& factoryJson);
    juce::Result write (SoundBank bank);
    std::shared_ptr<SoundBank> mutableCopy (const juce::String& id, juce::Result& result) const;

    juce::File directory;
    std::shared_ptr<const SoundBank> factory;
    std::vector<std::shared_ptr<const SoundBank>> banks; // user banks
    juce::StringArray scanErrors;
    std::unique_ptr<juce::PropertiesFile> settings;
    mutable juce::CriticalSection lock;
};

} // namespace mm
