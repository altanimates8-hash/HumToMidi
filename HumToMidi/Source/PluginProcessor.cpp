#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout HumToMidiProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Input Gate",
                                                        NormalisableRange<float> (-70.0f, -10.0f, 0.1f), -48.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "minNote", 1 }, "Note Start Time",
                                                        NormalisableRange<float> (20.0f, 300.0f, 1.0f), 60.0f,
                                                        AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "switch", 1 }, "Pitch Change Time",
                                                        NormalisableRange<float> (20.0f, 300.0f, 1.0f), 70.0f,
                                                        AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Note End Time",
                                                        NormalisableRange<float> (20.0f, 400.0f, 1.0f), 90.0f,
                                                        AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "transpose", 1 }, "Transpose", -24, 24, 0,
                                                      AudioParameterIntAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "velocity", 1 }, "Velocity", 0, 127, 0,
                                                      AudioParameterIntAttributes().withStringFromValueFunction (
                                                          [] (int v, int) { return v == 0 ? String ("Follow volume") : String (v); })));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mute", 1 }, "Mute Audio Output", false));

    return { p.begin(), p.end() };
}

HumToMidiProcessor::HumToMidiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pGate      = apvts.getRawParameterValue ("gate");
    pMinNote   = apvts.getRawParameterValue ("minNote");
    pSwitch    = apvts.getRawParameterValue ("switch");
    pRelease   = apvts.getRawParameterValue ("release");
    pTranspose = apvts.getRawParameterValue ("transpose");
    pVelocity  = apvts.getRawParameterValue ("velocity");
    pChannel   = apvts.getRawParameterValue ("channel");
    pMute      = apvts.getRawParameterValue ("mute");
}

bool HumToMidiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == layouts.getMainInputChannelSet()
           && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void HumToMidiProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
    monoScratch.assign (2048, 0.0f);
    displayNote = -1;
    displayHz = 0.0f;
    displayLevelDb = -100.0f;
}

void HumToMidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    auto& s = analyzer.settings;
    s.gateDb    = pGate->load();
    s.minNoteMs = pMinNote->load();
    s.switchMs  = pSwitch->load();
    s.releaseMs = pRelease->load();
    s.transpose = (int) std::lround (pTranspose->load());
    s.velocity  = (int) std::lround (pVelocity->load());
    const int channel = juce::jlimit (1, 16, (int) std::lround (pChannel->load()));

    const int chunkMax = (int) monoScratch.size();
    for (int start = 0; start < numSamples; start += chunkMax)
    {
        const int n = juce::jmin (chunkMax, numSamples - start);

        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numIn; ++ch)
                sum += buffer.getReadPointer (ch)[start + i];
            monoScratch[(size_t) i] = numIn > 0 ? sum / (float) numIn : 0.0f;
        }

        analyzer.process (monoScratch.data(), n, [&] (const hum::HumAnalyzer::Event& e) {
            const int pos = start + e.offset;
            if (e.on)
            {
                activeChannel = channel; // changes apply from the next note
                midi.addEvent (juce::MidiMessage::noteOn (activeChannel, e.note, (juce::uint8) e.velocity), pos);
            }
            else
                midi.addEvent (juce::MidiMessage::noteOff (activeChannel, e.note), pos);
        });
    }

    displayNote = analyzer.activeNote();
    displayHz = analyzer.lastFrequency();
    displayLevelDb = analyzer.lastLevel();

    if (pMute->load() > 0.5f)
        buffer.clear();
}

juce::AudioProcessorEditor* HumToMidiProcessor::createEditor()
{
    return new HumToMidiEditor (*this);
}

void HumToMidiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void HumToMidiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HumToMidiProcessor();
}
