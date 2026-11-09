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

#include "PluginProcessor.h"
#include "PluginEditor.h"

MiniMoopProcessor::MiniMoopProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("External In", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "MiniMoop", mm::params::createLayout())
{
    cache.attach (state);
    state.addParameterListener (mm::params::id::unitSeed, this);
    state.addParameterListener (mm::params::id::quality, this);
    engine.build (uint64_t (cache.unitSeed()));

    auto bank = library->getBank (library->getDefaultBankId());
    setActiveBankSnapshot (bank != nullptr ? bank : library->getFactory());
    setCurrentProgram (0); // a new instance starts on the first program (Init for Factory)
    library->addChangeListener (this);
}

MiniMoopProcessor::~MiniMoopProcessor()
{
    library->removeChangeListener (this);
    cancelPendingUpdate();
    state.removeParameterListener (mm::params::id::unitSeed, this);
    state.removeParameterListener (mm::params::id::quality, this);
}

bool MiniMoopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MiniMoopProcessor::prepareToPlay (double sr, int spb)
{
    sampleRate = sr;
    blockSize = std::max (16, spb);
    monoBuffer.assign (size_t (blockSize), 0.0f);
    inBuffer.assign (size_t (blockSize), 0.0f);
    engine.prepare (sampleRate, blockSize, cache.oversampling());
    keyboardState.reset();
}

void MiniMoopProcessor::parameterChanged (const juce::String&, float)
{
    // Rebuilding the units (component draw + calibration) or changing the
    // oversampling factor reallocates: do it off the audio thread.
    triggerAsyncUpdate();
}

void MiniMoopProcessor::handleAsyncUpdate()
{
    suspendProcessing (true);
    rebuildEngine();
    suspendProcessing (false);
}

void MiniMoopProcessor::rebuildEngine()
{
    engine.build (uint64_t (cache.unitSeed()));
    engine.prepare (sampleRate, blockSize, cache.oversampling());
}

void MiniMoopProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchWheel.store (float ((m.getPitchWheelValue() - 8192) / 8192.0));
    else if (m.isController())
    {
        switch (m.getControllerNumber())
        {
            case 1:  modWheel.store (float (m.getControllerValue()) / 127.0f); break;
            case 64: engine.sustainPedal (m.getControllerValue() >= 64); break;
            case 120:
            case 123: engine.allNotesOff(); break;
            default: break;
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void MiniMoopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(); // may exceed the announced size: renderTo() splits

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    cache.read (panel);
    panel.pitchWheel = pitchWheel.load();
    panel.modWheel = modWheel.load();
    engine.setVoiceMode (cache.voiceMode() == 1, cache.voices());
    engine.setPanel (panel);

    // Mono sum of the external input, if the host gives us one.
    const auto inBus = getBusBuffer (buffer, true, 0);
    const bool haveInput = getBus (true, 0) != nullptr && getBus (true, 0)->isEnabled() && inBus.getNumChannels() > 0;

    int pos = 0;
    auto renderTo = [&] (int end) {
        while (pos < end)
        {
            const int len = std::min (end - pos, int (monoBuffer.size()));
            const float* in = nullptr;
            if (haveInput)
            {
                for (int i = 0; i < len; ++i)
                {
                    float s = 0.0f;
                    for (int c = 0; c < inBus.getNumChannels(); ++c) s += inBus.getSample (c, pos + i);
                    inBuffer[size_t (i)] = s / float (inBus.getNumChannels());
                }
                in = inBuffer.data();
            }
            engine.process (monoBuffer.data(), in, len);
            auto outBus = getBusBuffer (buffer, false, 0);
            for (int c = 0; c < outBus.getNumChannels(); ++c)
                outBus.copyFrom (c, pos, monoBuffer.data(), len);
            pos += len;
        }
    };

    for (const auto meta : midi)
    {
        renderTo (std::min (meta.samplePosition, n));
        handleMidi (meta.getMessage());
    }
    renderTo (n);
    overloadLamp.store (float (engine.overloadLamp()));

    // Clear any extra (input-only) channels
    for (int c = getTotalNumOutputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* MiniMoopProcessor::createEditor()
{
    return new MiniMoopEditor (*this);
}

// ---------------------------------------------------------------------------- sound banks

std::shared_ptr<const mm::SoundBank> MiniMoopProcessor::getActiveBank() const
{
    const juce::SpinLock::ScopedLockType sl (bankLock);
    return activeBank;
}

void MiniMoopProcessor::setActiveBankSnapshot (std::shared_ptr<const mm::SoundBank> bank)
{
    jassert (bank != nullptr);
    const juce::SpinLock::ScopedLockType sl (bankLock);
    activeBank = std::move (bank);
}

bool MiniMoopProcessor::isInstrumentSetting (const juce::String& pid)
{
    // Kept across programs: they describe the instrument, not the sound.
    return pid == mm::params::id::unitSeed || pid == mm::params::id::quality || pid == mm::params::id::drift;
}

void MiniMoopProcessor::setActiveBank (const juce::String& id, bool makeDefault)
{
    auto bank = library->getBank (id);
    if (bank == nullptr) bank = library->getFactory();
    if (makeDefault) library->setDefaultBankId (bank->id);
    if (bank == getActiveBank()) return;
    setActiveBankSnapshot (std::move (bank));
    currentProgram = -1; // the sound is kept: it is not a program of the new bank
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void MiniMoopProcessor::loadPreset (const juce::String& bankId, int index)
{
    if (getActiveBank()->id != bankId)
        setActiveBank (bankId);
    setCurrentProgram (index);
}

void MiniMoopProcessor::reloadActiveBank (int loadedProgram)
{
    if (auto bank = library->getBank (getActiveBank()->id))
        setActiveBankSnapshot (std::move (bank));
    currentProgram = juce::isPositiveAndBelow (loadedProgram, getActiveBank()->numPresets()) ? loadedProgram : -1;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void MiniMoopProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A bank was edited, possibly from another instance. A deleted bank stays
    // usable here until another one is chosen.
    auto current = getActiveBank();
    auto fresh = library->getBank (current->id);
    if (fresh == nullptr || fresh == current) return;
    setActiveBankSnapshot (fresh);
    if (currentProgram >= fresh->numPresets()) currentProgram = -1;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

mm::PresetValues MiniMoopProcessor::captureCurrentValues() const
{
    mm::PresetValues values;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isInstrumentSetting (rp->getParameterID()))
                values.emplace_back (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()));
    return values;
}

int MiniMoopProcessor::getNumPrograms() { return std::max (1, getActiveBank()->numPresets()); }

const juce::String MiniMoopProcessor::getProgramName (int index)
{
    const auto bank = getActiveBank();
    const auto* preset = bank->presetAt (index);
    return preset != nullptr ? preset->name : juce::String();
}

void MiniMoopProcessor::setCurrentProgram (int index)
{
    const auto bank = getActiveBank();
    const auto* preset = bank->presetAt (index);
    if (preset == nullptr)
        return;
    currentProgram = index;
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        if (isInstrumentSetting (pid))
            continue;
        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        for (const auto& [key, v] : preset->values)
            if (pid == key) value = v;
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
}

void MiniMoopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = state.copyState();
    s.setProperty ("program", currentProgram.load(), nullptr);
    s.setProperty ("bank", getActiveBank()->id, nullptr);
    if (auto xml = s.createXml())
        copyXmlToBinary (*xml, destData);
}

void MiniMoopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (state.state.getType()))
        {
            auto s = juce::ValueTree::fromXml (*xml);
            // Older sessions have no bank: their programs were the factory ones,
            // numbered in the order they had before categories existed.
            const bool legacy = ! s.hasProperty ("bank");
            const auto bankId = s.getProperty ("bank", mm::BankLibrary::factoryId).toString();
            auto bank = library->getBank (bankId);
            int program = s.getProperty ("program", 0);
            if (legacy && bank != nullptr)
            {
                static const char* const legacyOrder[] = { "Init", "Fat Bass", "Solo Lead", "Self-Osc Whistle",
                                                           "Feedback Growl", "Osc 3 Wobble", "Poly Brass", "Noise Wind" };
                const juce::String name = juce::isPositiveAndBelow (program, int (std::size (legacyOrder))) ? legacyOrder[program] : "";
                program = -1;
                for (int i = 0; i < bank->numPresets(); ++i)
                    if (bank->presetAt (i)->name == name) program = i;
            }
            if (bank == nullptr) // bank gone: keep the sound, fall back to Factory
            {
                bank = library->getFactory();
                program = -1;
            }
            setActiveBankSnapshot (bank);
            currentProgram = juce::isPositiveAndBelow (program, bank->numPresets()) ? program : -1;
            s.removeProperty ("bank", nullptr);
            s.removeProperty ("program", nullptr);
            state.replaceState (s);
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiniMoopProcessor();
}
