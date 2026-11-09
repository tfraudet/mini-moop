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

#include "Parameters.h"
#include "SoundBank.h"
#include "engine/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MiniMoopProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater,
                          private juce::ChangeListener
{
public:
    MiniMoopProcessor();
    ~MiniMoopProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mini Moop"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    /** Hosts expect a valid index: 0 when the sound is not a program of the active bank. */
    int getCurrentProgram() override { return std::max (0, currentProgram.load()); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboardState;

    /** Performance wheels: written by MIDI (audio thread) or by the on-screen
        wheels (message thread). Not automatable host parameters, like the
        real spring-loaded / free wheels. */
    std::atomic<float> pitchWheel { 0.0f }, modWheel { 0.0f };
    std::atomic<float> overloadLamp { 0.0f }; // OVERLOAD lamp brightness for the panel

    // ---- sound banks (message thread, except the getters used by hosts)
    mm::BankLibrary& getLibrary() { return *library; }
    std::shared_ptr<const mm::SoundBank> getActiveBank() const;
    /** Program of the active bank the panel was loaded from, -1 if none
        (bank just changed, or the program was deleted). */
    int getLoadedProgram() const noexcept { return currentProgram.load(); }
    /** Makes `id` the active bank without changing the sound. Unless
        `makeDefault` is false, new instances will also start with it. */
    void setActiveBank (const juce::String& id, bool makeDefault = true);
    /** Activates `bankId` if needed, then loads its program `index`. */
    void loadPreset (const juce::String& bankId, int index);
    /** Re-reads the active bank from the library after it was edited and
        sets the loaded program (-1: none). */
    void reloadActiveBank (int loadedProgram);
    /** Current panel settings in parameter units, without the instrument-level
        settings (unit, quality, drift). */
    mm::PresetValues captureCurrentValues() const;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void rebuildEngine();
    void handleMidi (const juce::MidiMessage& m);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setActiveBankSnapshot (std::shared_ptr<const mm::SoundBank> bank);
    static bool isInstrumentSetting (const juce::String& parameterId);

    mm::SynthEngine engine;
    mm::params::ParameterCache cache;
    mm::PanelState panel;
    double sampleRate = 48000.0;
    int blockSize = 512;
    juce::SharedResourcePointer<mm::BankLibrary> library;
    std::shared_ptr<const mm::SoundBank> activeBank;
    juce::SpinLock bankLock;
    std::atomic<int> currentProgram { -1 };
    std::vector<float> monoBuffer, inBuffer;
};
