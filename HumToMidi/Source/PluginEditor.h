#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class HumToMidiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HumToMidiEditor (HumToMidiProcessor&);
    ~HumToMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addRow (const juce::String& paramId, const juce::String& text);

    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    HumToMidiProcessor& processor;
    std::vector<std::unique_ptr<Row>> rows;
    juce::ToggleButton muteButton { "Mute audio output" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;

    int shownNote = -2;
    float shownHz = -1.0f, shownLevel = -200.0f;

    static constexpr int displayHeight = 150;
    static constexpr int rowHeight = 34;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumToMidiEditor)
};
