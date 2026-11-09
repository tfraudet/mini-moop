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

#include "SoundBank.h"

#include "MiniMoopAssets.h"

namespace mm
{

// ============================================================================ SoundBank

namespace
{
juce::Result fail (const juce::String& message) { return juce::Result::fail (message); }

juce::var valueToVar (float v)
{
    const double rounded = std::round (double (v) * 10000.0) / 10000.0;
    if (rounded == std::round (rounded) && std::abs (rounded) < 1.0e6)
        return juce::var (int (rounded));
    return juce::var (rounded);
}

bool sameName (const juce::String& a, const juce::String& b) { return a.trim().equalsIgnoreCase (b.trim()); }
} // namespace

juce::Result SoundBank::fromJson (const juce::String& json, SoundBank& out)
{
    juce::var root;
    if (const auto parsed = juce::JSON::parse (json, root); parsed.failed())
        return fail ("The file is not valid JSON (" + parsed.getErrorMessage() + ").");
    if (! root.isObject())
        return fail ("The file is not a Mini Moop sound bank.");
    if (root["format"].toString() != formatTag)
        return fail ("The file is not a Mini Moop sound bank (\"format\" should be \"" + juce::String (formatTag) + "\").");
    if (! (root["version"].isInt() || root["version"].isInt64() || root["version"].isDouble()))
        return fail ("The bank has no format version.");
    if (int (root["version"]) > formatVersion)
        return fail ("The bank was made by a newer version of Mini Moop (format version "
                     + root["version"].toString() + ").");

    SoundBank bank;
    bank.name = root["name"].toString().trim();
    if (bank.name.isEmpty())
        return fail ("The bank has no name.");
    bank.author = root["author"].toString();
    bank.description = root["description"].toString();

    const auto* categories = root["categories"].getArray();
    if (categories == nullptr)
        return fail ("The bank has no \"categories\" list.");

    for (const auto& c : *categories)
    {
        BankCategory category;
        category.name = c["name"].toString().trim();
        if (! c.isObject() || category.name.isEmpty())
            return fail ("A category has no name.");
        const auto* presets = c["presets"].getArray();
        if (presets == nullptr)
            return fail ("Category \"" + category.name + "\" has no \"presets\" list.");

        for (const auto& p : *presets)
        {
            BankPreset preset;
            preset.name = p["name"].toString().trim();
            if (! p.isObject() || preset.name.isEmpty())
                return fail ("A preset of category \"" + category.name + "\" has no name.");
            const auto& values = p["values"];
            if (! values.isVoid() && ! values.isObject())
                return fail ("Preset \"" + preset.name + "\": \"values\" should be an object.");
            if (auto* obj = values.getDynamicObject())
                for (const auto& [key, v] : obj->getProperties())
                {
                    if (! (v.isInt() || v.isInt64() || v.isDouble() || v.isBool()))
                        return fail ("Preset \"" + preset.name + "\": the value of \"" + key.toString() + "\" is not a number.");
                    preset.values.emplace_back (key.toString(), float (double (v)));
                }
            category.presets.push_back (std::move (preset));
        }
        bank.categories.push_back (std::move (category));
    }

    out.name = std::move (bank.name);
    out.author = std::move (bank.author);
    out.description = std::move (bank.description);
    out.categories = std::move (bank.categories);
    return juce::Result::ok();
}

juce::String SoundBank::toJson() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("format", formatTag);
    root->setProperty ("version", formatVersion);
    root->setProperty ("name", name);
    root->setProperty ("author", author);
    root->setProperty ("description", description);

    juce::Array<juce::var> cats;
    for (const auto& c : categories)
    {
        auto cat = std::make_unique<juce::DynamicObject>();
        cat->setProperty ("name", c.name);
        juce::Array<juce::var> presets;
        for (const auto& p : c.presets)
        {
            auto values = std::make_unique<juce::DynamicObject>();
            for (const auto& [key, v] : p.values)
                values->setProperty (key, valueToVar (v));
            auto preset = std::make_unique<juce::DynamicObject>();
            preset->setProperty ("name", p.name);
            preset->setProperty ("values", juce::var (values.release()));
            presets.add (juce::var (preset.release()));
        }
        cat->setProperty ("presets", presets);
        cats.add (juce::var (cat.release()));
    }
    root->setProperty ("categories", cats);

    return juce::JSON::toString (juce::var (root.release()),
                                 juce::JSON::FormatOptions().withSpacing (juce::JSON::Spacing::multiLine)
                                                            .withMaxDecimalPlaces (4)) + "\n";
}

int SoundBank::numPresets() const noexcept
{
    int n = 0;
    for (const auto& c : categories) n += int (c.presets.size());
    return n;
}

SoundBank::Location SoundBank::locate (int flatIndex) const noexcept
{
    if (flatIndex < 0) return {};
    for (int c = 0; c < int (categories.size()); ++c)
    {
        const int n = int (categories[size_t (c)].presets.size());
        if (flatIndex < n) return { c, flatIndex };
        flatIndex -= n;
    }
    return {};
}

const BankPreset* SoundBank::presetAt (int flatIndex) const noexcept
{
    const auto l = locate (flatIndex);
    return l.category < 0 ? nullptr : &categories[size_t (l.category)].presets[size_t (l.preset)];
}

const BankCategory* SoundBank::categoryOf (int flatIndex) const noexcept
{
    const auto l = locate (flatIndex);
    return l.category < 0 ? nullptr : &categories[size_t (l.category)];
}

int SoundBank::flatIndexOf (int category, int preset) const noexcept
{
    if (! juce::isPositiveAndBelow (category, int (categories.size()))
        || ! juce::isPositiveAndBelow (preset, int (categories[size_t (category)].presets.size())))
        return -1;
    int n = 0;
    for (int c = 0; c < category; ++c) n += int (categories[size_t (c)].presets.size());
    return n + preset;
}

int SoundBank::find (const juce::String& category, const juce::String& presetName) const
{
    for (int c = 0; c < int (categories.size()); ++c)
        if (sameName (categories[size_t (c)].name, category))
            for (int p = 0; p < int (categories[size_t (c)].presets.size()); ++p)
                if (sameName (categories[size_t (c)].presets[size_t (p)].name, presetName))
                    return flatIndexOf (c, p);
    return -1;
}

// ============================================================================ BankLibrary

juce::File BankLibrary::defaultDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Zazart").getChildFile ("Mini Moop").getChildFile ("presets");
}

BankLibrary::BankLibrary()
    : BankLibrary (defaultDirectory(), juce::String::fromUTF8 (MiniMoopAssets::Factory_json, MiniMoopAssets::Factory_jsonSize))
{
}

BankLibrary::BankLibrary (const juce::File& dir, const juce::String& factoryJson) : directory (dir)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Mini Moop";
    options.filenameSuffix = ".settings";
    options.millisecondsBeforeSaving = 0;
    settings = std::make_unique<juce::PropertiesFile> (directory.getParentDirectory().getChildFile ("Mini Moop.settings"), options);

    installFactory (factoryJson);
    rescan();
}

void BankLibrary::installFactory (const juce::String& factoryJson)
{
    auto bank = std::make_shared<SoundBank>();
    if (SoundBank::fromJson (factoryJson, *bank).failed())
    {
        jassertfalse; // the built-in bank must always parse
        bank->name = factoryId;
        bank->categories = { { "Basic", { { "Init", {} } } } };
    }
    bank->id = factoryId;
    bank->readOnly = true;
    bank->file = directory.getChildFile (juce::String (factoryId) + ".json");

    // Mirror the built-in bank to disk so it can be seen, copied and exported.
    // The built-in version always wins over an edited copy.
    if (directory.createDirectory().wasOk()
        && (! bank->file.existsAsFile() || bank->file.loadFileAsString() != factoryJson))
        bank->file.replaceWithText (factoryJson, false, false, "\n");

    const juce::ScopedLock sl (lock);
    factory = std::move (bank);
}

void BankLibrary::rescan()
{
    std::vector<std::shared_ptr<const SoundBank>> found;
    juce::StringArray errors;
    for (const auto& f : directory.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        if (f.getFileNameWithoutExtension().equalsIgnoreCase (factoryId))
            continue;
        auto bank = std::make_shared<SoundBank>();
        if (const auto r = SoundBank::fromJson (f.loadFileAsString(), *bank); r.failed())
        {
            errors.add (f.getFileName() + ": " + r.getErrorMessage());
            continue;
        }
        bank->id = f.getFileNameWithoutExtension();
        bank->file = f;
        found.push_back (std::move (bank));
    }
    std::sort (found.begin(), found.end(), [] (const auto& a, const auto& b) {
        const int byName = a->name.compareNatural (b->name);
        return byName != 0 ? byName < 0 : a->id.compareNatural (b->id) < 0;
    });

    {
        const juce::ScopedLock sl (lock);
        banks = std::move (found);
        scanErrors = errors;
    }
    sendChangeMessage();
}

std::vector<std::shared_ptr<const SoundBank>> BankLibrary::getBanks() const
{
    const juce::ScopedLock sl (lock);
    std::vector<std::shared_ptr<const SoundBank>> all { factory };
    all.insert (all.end(), banks.begin(), banks.end());
    return all;
}

std::shared_ptr<const SoundBank> BankLibrary::getBank (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    if (id == factoryId) return factory;
    for (const auto& b : banks)
        if (b->id == id) return b;
    return nullptr;
}

juce::StringArray BankLibrary::getScanErrors() const
{
    const juce::ScopedLock sl (lock);
    return scanErrors;
}

juce::String BankLibrary::uniqueId (const juce::String& name) const
{
    auto base = juce::File::createLegalFileName (name.trim()).trim().trimCharactersAtStart (".");
    if (base.isEmpty()) base = "Bank";

    auto taken = [&] (const juce::String& id) {
        if (id.equalsIgnoreCase (factoryId) || directory.getChildFile (id + ".json").exists())
            return true;
        const juce::ScopedLock sl (lock);
        for (const auto& b : banks)
            if (b->id.equalsIgnoreCase (id)) return true;
        return false;
    };

    auto id = base;
    for (int n = 2; taken (id); ++n)
        id = base + " " + juce::String (n);
    return id;
}

juce::Result BankLibrary::write (SoundBank bank)
{
    if (! directory.createDirectory())
        return fail ("Cannot create the banks folder " + directory.getFullPathName() + ".");

    bank.readOnly = false;
    bank.file = directory.getChildFile (bank.id + ".json");
    juce::TemporaryFile temp (bank.file);
    if (! temp.getFile().replaceWithText (bank.toJson(), false, false, "\n") || ! temp.overwriteTargetFileWithTemporary())
        return fail ("Cannot write " + bank.file.getFullPathName() + ".");

    {
        const juce::ScopedLock sl (lock);
        auto shared = std::make_shared<const SoundBank> (std::move (bank));
        auto it = std::find_if (banks.begin(), banks.end(), [&] (const auto& b) { return b->id == shared->id; });
        if (it != banks.end()) *it = shared;
        else banks.push_back (shared);
        std::sort (banks.begin(), banks.end(), [] (const auto& a, const auto& b) {
            const int byName = a->name.compareNatural (b->name);
            return byName != 0 ? byName < 0 : a->id.compareNatural (b->id) < 0;
        });
    }
    sendChangeMessage();
    return juce::Result::ok();
}

std::shared_ptr<SoundBank> BankLibrary::mutableCopy (const juce::String& id, juce::Result& result) const
{
    if (id == factoryId)
    {
        result = fail ("The Factory bank is read-only. Duplicate it or create a new bank to save your own sounds.");
        return nullptr;
    }
    auto bank = getBank (id);
    if (bank == nullptr)
    {
        result = fail ("The bank \"" + id + "\" no longer exists.");
        return nullptr;
    }
    result = juce::Result::ok();
    return std::make_shared<SoundBank> (*bank);
}

juce::Result BankLibrary::importBank (const juce::File& source, juce::String& newId)
{
    if (! source.existsAsFile())
        return fail ("The file " + source.getFullPathName() + " does not exist.");
    SoundBank bank;
    if (const auto r = SoundBank::fromJson (source.loadFileAsString(), bank); r.failed())
        return fail (source.getFileName() + ": " + r.getErrorMessage());

    bank.id = uniqueId (bank.name);
    // A second copy of a bank gets a distinct name too, so both can be told apart.
    if (bank.id != juce::File::createLegalFileName (bank.name.trim()).trim())
        bank.name = bank.id;
    newId = bank.id;
    return write (std::move (bank));
}

juce::Result BankLibrary::exportBank (const juce::String& id, const juce::File& destination) const
{
    const auto bank = getBank (id);
    if (bank == nullptr)
        return fail ("The bank \"" + id + "\" no longer exists.");
    if (bank->file.existsAsFile() && bank->file != destination)
    {
        if (! bank->file.copyFileTo (destination))
            return fail ("Cannot write " + destination.getFullPathName() + ".");
    }
    else if (! destination.replaceWithText (bank->toJson(), false, false, "\n"))
        return fail ("Cannot write " + destination.getFullPathName() + ".");
    return juce::Result::ok();
}

juce::Result BankLibrary::createBank (const juce::String& name, juce::String& newId)
{
    if (name.trim().isEmpty())
        return fail ("Please give the bank a name.");
    SoundBank bank;
    bank.name = name.trim();
    bank.id = uniqueId (bank.name);
    newId = bank.id;
    return write (std::move (bank));
}

juce::Result BankLibrary::duplicateBank (const juce::String& id, juce::String& newId)
{
    const auto source = getBank (id);
    if (source == nullptr)
        return fail ("The bank \"" + id + "\" no longer exists.");
    SoundBank bank (*source);
    bank.name = source->name + " copy";
    bank.id = uniqueId (bank.name);
    bank.name = bank.id;
    newId = bank.id;
    return write (std::move (bank));
}

juce::Result BankLibrary::renameBank (const juce::String& id, const juce::String& newName, juce::String& newId)
{
    if (newName.trim().isEmpty())
        return fail ("Please give the bank a name.");
    auto r = juce::Result::ok();
    auto bank = mutableCopy (id, r);
    if (bank == nullptr) return r;

    const auto oldFile = bank->file;
    bank->name = newName.trim();
    const auto wanted = juce::File::createLegalFileName (bank->name).trim();
    newId = wanted.equalsIgnoreCase (id) ? id : uniqueId (bank->name);
    bank->id = newId;
    if (r = write (*bank); r.failed())
        return r;

    if (newId != id)
    {
        oldFile.deleteFile();
        {
            const juce::ScopedLock sl (lock);
            banks.erase (std::remove_if (banks.begin(), banks.end(), [&] (const auto& b) { return b->id == id; }), banks.end());
        }
        if (getDefaultBankId() == id) setDefaultBankId (newId);
        sendChangeMessage();
    }
    return juce::Result::ok();
}

juce::Result BankLibrary::deleteBank (const juce::String& id)
{
    auto r = juce::Result::ok();
    auto bank = mutableCopy (id, r);
    if (bank == nullptr) return r;

    if (bank->file.existsAsFile() && ! bank->file.moveToTrash() && ! bank->file.deleteFile())
        return fail ("Cannot delete " + bank->file.getFullPathName() + ".");
    {
        const juce::ScopedLock sl (lock);
        banks.erase (std::remove_if (banks.begin(), banks.end(), [&] (const auto& b) { return b->id == id; }), banks.end());
    }
    if (getDefaultBankId() == id) setDefaultBankId (factoryId);
    sendChangeMessage();
    return juce::Result::ok();
}

juce::Result BankLibrary::savePreset (const juce::String& bankId, const juce::String& categoryName,
                                      const juce::String& name, const PresetValues& values, int& flatIndex)
{
    if (name.trim().isEmpty())
        return fail ("Please give the preset a name.");
    auto r = juce::Result::ok();
    auto bank = mutableCopy (bankId, r);
    if (bank == nullptr) return r;

    const auto catName = categoryName.trim().isEmpty() ? juce::String ("User") : categoryName.trim();
    auto cat = std::find_if (bank->categories.begin(), bank->categories.end(),
                             [&] (const auto& c) { return sameName (c.name, catName); });
    if (cat == bank->categories.end())
    {
        bank->categories.push_back ({ catName, {} });
        cat = std::prev (bank->categories.end());
    }
    auto preset = std::find_if (cat->presets.begin(), cat->presets.end(),
                                [&] (const auto& p) { return sameName (p.name, name); });
    if (preset != cat->presets.end())
        preset->values = values;
    else
    {
        cat->presets.push_back ({ name.trim(), values });
        preset = std::prev (cat->presets.end());
    }
    flatIndex = bank->flatIndexOf (int (cat - bank->categories.begin()), int (preset - cat->presets.begin()));
    return write (*bank);
}

juce::Result BankLibrary::renamePreset (const juce::String& bankId, int flatIndex, const juce::String& newName)
{
    if (newName.trim().isEmpty())
        return fail ("Please give the preset a name.");
    auto r = juce::Result::ok();
    auto bank = mutableCopy (bankId, r);
    if (bank == nullptr) return r;
    const auto l = bank->locate (flatIndex);
    if (l.category < 0)
        return fail ("This preset no longer exists.");
    bank->categories[size_t (l.category)].presets[size_t (l.preset)].name = newName.trim();
    return write (*bank);
}

juce::Result BankLibrary::deletePreset (const juce::String& bankId, int flatIndex)
{
    auto r = juce::Result::ok();
    auto bank = mutableCopy (bankId, r);
    if (bank == nullptr) return r;
    const auto l = bank->locate (flatIndex);
    if (l.category < 0)
        return fail ("This preset no longer exists.");
    auto& cat = bank->categories[size_t (l.category)];
    cat.presets.erase (cat.presets.begin() + l.preset);
    if (cat.presets.empty())
        bank->categories.erase (bank->categories.begin() + l.category);
    return write (*bank);
}

juce::String BankLibrary::getDefaultBankId() const
{
    return settings->getValue ("defaultBank", factoryId);
}

void BankLibrary::setDefaultBankId (const juce::String& id)
{
    settings->setValue ("defaultBank", id);
}

} // namespace mm
