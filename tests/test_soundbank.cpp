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

// Sound banks: JSON format, built-in Factory bank, library operations on disk.

#include "LegacyPresets.h"
#include "MiniMoopAssets.h"
#include "SoundBank.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace
{
juce::String factoryJson() { return juce::String::fromUTF8 (MiniMoopAssets::Factory_json, MiniMoopAssets::Factory_jsonSize); }

/** Fresh banks folder in a temporary directory, deleted at the end of the test. */
struct TempLibrary
{
    juce::TemporaryFile root { "minimoop_banks" };
    juce::File dir = root.getFile().getChildFile ("presets");
    std::unique_ptr<mm::BankLibrary> lib;

    TempLibrary() { lib = std::make_unique<mm::BankLibrary> (dir, factoryJson()); }
    ~TempLibrary()
    {
        lib.reset();
        root.getFile().deleteRecursively();
    }
};
} // namespace

TEST_CASE ("Factory bank parses with the expected categories", "[banks]")
{
    mm::SoundBank bank;
    const auto r = mm::SoundBank::fromJson (factoryJson(), bank);
    INFO (r.getErrorMessage());
    REQUIRE (r.wasOk());
    CHECK (bank.name == "Factory");
    CHECK (bank.numPresets() == mm::legacy::numFactoryPresets);
    REQUIRE (bank.categories.size() == 5);
    CHECK (bank.categories[0].name == "Basic");
    CHECK (bank.presetAt (0)->name == "Init");
    CHECK (bank.presetAt (0)->values.empty());
}

TEST_CASE ("Factory bank holds the former hard-coded programs", "[banks]")
{
    mm::SoundBank bank;
    REQUIRE (mm::SoundBank::fromJson (factoryJson(), bank).wasOk());
    for (const auto& legacy : mm::legacy::factoryPresets)
    {
        INFO (legacy.name);
        const mm::BankPreset* found = nullptr;
        for (int i = 0; i < bank.numPresets(); ++i)
            if (bank.presetAt (i)->name == legacy.name) found = bank.presetAt (i);
        REQUIRE (found != nullptr);
        REQUIRE (found->values.size() == legacy.values.size());
        size_t k = 0;
        for (const auto& [key, value] : legacy.values)
        {
            CHECK (found->values[k].first == key);
            CHECK (std::abs (found->values[k].second - value) < 1.0e-5f);
            ++k;
        }
    }
}

TEST_CASE ("toJson / fromJson round trip", "[banks]")
{
    mm::SoundBank bank;
    bank.name = "Test";
    bank.author = "Me";
    bank.description = "Round trip";
    bank.categories = { { "Bass", { { "A", { { "cutoff", 3.5f }, { "osc2Freq", 0.08f }, { "osc1Range", 2.0f } } } } },
                        { "Lead", { { "B", {} }, { "C", { { "emphasis", -1.25f } } } } } };

    mm::SoundBank back;
    REQUIRE (mm::SoundBank::fromJson (bank.toJson(), back).wasOk());
    CHECK (back.name == "Test");
    CHECK (back.author == "Me");
    CHECK (back.description == "Round trip");
    REQUIRE (back.numPresets() == 3);
    CHECK (back.flatIndexOf (1, 1) == 2);
    CHECK (back.presetAt (2)->name == "C");
    CHECK (back.categoryOf (2)->name == "Lead");
    REQUIRE (back.presetAt (0)->values.size() == 3);
    CHECK (back.presetAt (0)->values[1].first == "osc2Freq");
    CHECK (back.presetAt (0)->values[1].second == 0.08f);
    CHECK (back.find ("lead", "c") == 2);
    CHECK (back.find ("Bass", "C") == -1);
    CHECK (back.presetAt (3) == nullptr);
    CHECK (back.presetAt (-1) == nullptr);
    // Integral values are written without decimals
    CHECK (bank.toJson().contains ("\"osc1Range\": 2"));
    CHECK (! bank.toJson().contains ("\"osc1Range\": 2."));
    CHECK (bank.toJson().contains ("\"osc2Freq\": 0.08,"));
}

TEST_CASE ("Invalid banks are rejected with a message", "[banks]")
{
    mm::SoundBank bank;
    auto check = [&] (const char* json, const char* expected) {
        const auto r = mm::SoundBank::fromJson (json, bank);
        INFO (json);
        REQUIRE (r.failed());
        CHECK_THAT (r.getErrorMessage().toStdString(), Catch::Matchers::ContainsSubstring (expected));
    };
    check ("{ not json", "not valid JSON");
    check ("[1, 2]", "not a Mini Moop sound bank");
    check (R"({ "format": "Other", "version": 1, "name": "X", "categories": [] })", "not a Mini Moop sound bank");
    check (R"({ "format": "MiniMoopBank", "version": 99, "name": "X", "categories": [] })", "newer version");
    check (R"({ "format": "MiniMoopBank", "version": 1, "name": "", "categories": [] })", "no name");
    check (R"({ "format": "MiniMoopBank", "version": 1, "name": "X" })", "categories");
    check (R"({ "format": "MiniMoopBank", "version": 1, "name": "X", "categories": [ { "name": "C", "presets": [ { "name": "P", "values": { "cutoff": "high" } } ] } ] })",
           "not a number");
}

TEST_CASE ("Library mirrors Factory to disk and protects it", "[banks]")
{
    TempLibrary t;
    auto& lib = *t.lib;
    const auto factoryFile = t.dir.getChildFile ("Factory.json");
    REQUIRE (factoryFile.existsAsFile());
    CHECK (factoryFile.loadFileAsString() == factoryJson());
    CHECK (lib.getBanks().size() == 1);
    CHECK (lib.getFactory()->readOnly);
    CHECK (lib.getDefaultBankId() == "Factory");

    int flat = -1;
    CHECK (lib.savePreset ("Factory", "Bass", "X", {}, flat).failed());
    CHECK (lib.deleteBank ("Factory").failed());
    juce::String id;
    CHECK (lib.renameBank ("Factory", "Other", id).failed());

    // an edited copy on disk is restored by the built-in one
    factoryFile.replaceWithText ("{}");
    mm::BankLibrary again (t.dir, factoryJson());
    CHECK (factoryFile.loadFileAsString() == factoryJson());
}

TEST_CASE ("Library bank and preset operations", "[banks]")
{
    TempLibrary t;
    auto& lib = *t.lib;

    juce::String id;
    REQUIRE (lib.createBank ("My Bank", id).wasOk());
    CHECK (id == "My Bank");
    CHECK (t.dir.getChildFile ("My Bank.json").existsAsFile());

    int flat = -1;
    REQUIRE (lib.savePreset (id, "Bass", "Deep", { { "cutoff", 2.0f } }, flat).wasOk());
    CHECK (flat == 0);
    REQUIRE (lib.savePreset (id, "Lead", "Sharp", { { "cutoff", 7.0f } }, flat).wasOk());
    CHECK (flat == 1);
    REQUIRE (lib.savePreset (id, "Bass", "Sub", { { "cutoff", 1.0f } }, flat).wasOk());
    CHECK (flat == 1); // inserted in Bass, before Lead
    // same name + category replaces
    REQUIRE (lib.savePreset (id, "bass", "deep", { { "cutoff", 2.5f } }, flat).wasOk());
    CHECK (flat == 0);
    auto bank = lib.getBank (id);
    REQUIRE (bank->numPresets() == 3);
    CHECK (bank->presetAt (0)->values[0].second == 2.5f);
    CHECK (bank->presetAt (2)->name == "Sharp");

    REQUIRE (lib.renamePreset (id, 2, "Sharper").wasOk());
    REQUIRE (lib.deletePreset (id, 2).wasOk()); // Lead becomes empty and goes away
    bank = lib.getBank (id);
    CHECK (bank->numPresets() == 2);
    CHECK (bank->categories.size() == 1);

    // a new library instance reads the same content back from disk
    {
        mm::BankLibrary other (t.dir, factoryJson());
        REQUIRE (other.getBank (id) != nullptr);
        CHECK (other.getBank (id)->numPresets() == 2);
    }

    juce::String copyId;
    REQUIRE (lib.duplicateBank (id, copyId).wasOk());
    CHECK (copyId == "My Bank copy");
    CHECK (lib.getBank (copyId)->numPresets() == 2);

    lib.setDefaultBankId (id);
    juce::String renamed;
    REQUIRE (lib.renameBank (id, "Live Set", renamed).wasOk());
    CHECK (renamed == "Live Set");
    CHECK (lib.getBank (id) == nullptr);
    CHECK (! t.dir.getChildFile ("My Bank.json").exists());
    CHECK (lib.getDefaultBankId() == "Live Set");

    REQUIRE (lib.deleteBank (renamed).wasOk());
    CHECK (lib.getBank (renamed) == nullptr);
    CHECK (lib.getDefaultBankId() == "Factory");
    CHECK (lib.getBanks().size() == 2); // Factory + copy
}

TEST_CASE ("Import and export", "[banks]")
{
    TempLibrary t;
    auto& lib = *t.lib;

    const auto exported = t.root.getFile().getChildFile ("exported.json");
    REQUIRE (lib.exportBank ("Factory", exported).wasOk());
    CHECK (exported.loadFileAsString() == factoryJson());

    // importing a bank named "Factory" never replaces the built-in one
    juce::String id;
    REQUIRE (lib.importBank (exported, id).wasOk());
    CHECK (id == "Factory 2");
    CHECK (lib.getBank (id)->name == "Factory 2");
    CHECK (! lib.getBank (id)->readOnly);
    CHECK (lib.getBank (id)->numPresets() == lib.getFactory()->numPresets());

    juce::String id2;
    REQUIRE (lib.importBank (exported, id2).wasOk());
    CHECK (id2 == "Factory 3");

    const auto bad = t.root.getFile().getChildFile ("bad.json");
    bad.replaceWithText ("{ \"hello\": 1 }");
    juce::String id3;
    const auto r = lib.importBank (bad, id3);
    CHECK (r.failed());
    CHECK (r.getErrorMessage().contains ("bad.json"));

    // unreadable files in the folder are reported, not loaded
    t.dir.getChildFile ("broken.json").replaceWithText ("nope");
    lib.rescan();
    CHECK (lib.getScanErrors().size() == 1);
    CHECK (lib.getBanks().size() == 3);
}
