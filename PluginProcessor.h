#pragma once
#include "HumAnalyzer.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class HumToMidiProcessor : public juce::AudioProcessor
{
public:
    HumToMidiProcessor();
    ~HumToMidiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HumToMidi"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor
    std::atomic<int>   displayNote { -1 };
    std::atomic<float> displayHz { 0.0f };
    std::atomic<float> displayLevelDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    hum::HumAnalyzer analyzer;
    std::vector<float> monoScratch;
    int activeChannel = 1;

    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pMinNote = nullptr;
    std::atomic<float>* pSwitch = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pVelocity = nullptr;
    std::atomic<float>* pChannel = nullptr;
    std::atomic<float>* pMute = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumToMidiProcessor)
};
