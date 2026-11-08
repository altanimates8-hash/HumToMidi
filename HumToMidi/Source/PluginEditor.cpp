#include "PluginEditor.h"

namespace
{
const juce::Colour bg     { 0xff14171c };
const juce::Colour panel  { 0xff1d2128 };
const juce::Colour accent { 0xff4cc9f0 };
const juce::Colour dim    { 0xff8a93a0 };

juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}
} // namespace

HumToMidiEditor::HumToMidiEditor (HumToMidiProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addRow ("gate", "Input gate");
    addRow ("minNote", "Note start time");
    addRow ("switch", "Pitch change time");
    addRow ("release", "Note end time");
    addRow ("transpose", "Transpose");
    addRow ("velocity", "Velocity");
    addRow ("channel", "MIDI channel");

    muteButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    muteButton.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (muteButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "mute", muteButton);

    setSize (460, displayHeight + (int) rows.size() * rowHeight + 60);
    startTimerHz (30);
}

HumToMidiEditor::~HumToMidiEditor() = default;

void HumToMidiEditor::addRow (const juce::String& paramId, const juce::String& text)
{
    auto row = std::make_unique<Row>();
    row->label.setText (text, juce::dontSendNotification);
    row->label.setColour (juce::Label::textColourId, juce::Colours::white);
    row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 100, 22);
    row->slider.setColour (juce::Slider::trackColourId, accent);
    row->slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    row->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    row->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    row->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, row->slider);
    addAndMakeVisible (row->label);
    addAndMakeVisible (row->slider);
    rows.push_back (std::move (row));
}

void HumToMidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto area = getLocalBounds().removeFromTop (displayHeight).reduced (12);
    g.setColour (panel);
    g.fillRoundedRectangle (area.toFloat(), 10.0f);

    auto inner = area.reduced (16, 12);
    const int note = processor.displayNote.load();
    const float hz = processor.displayHz.load();
    const float db = processor.displayLevelDb.load();

    g.setColour (note >= 0 ? accent : dim);
    g.setFont (juce::FontOptions (54.0f, juce::Font::bold));
    g.drawText (note >= 0 ? noteName (note) : "--", inner.removeFromTop (62), juce::Justification::centred);

    g.setColour (dim);
    g.setFont (juce::FontOptions (14.0f));
    juce::String info = note >= 0 ? "MIDI note " + juce::String (note) : "Hum into your mic";
    if (hz > 0.0f)
        info << "   |   " << juce::String (hz, 1) << " Hz";
    g.drawText (info, inner.removeFromTop (20), juce::Justification::centred);

    // level meter, with the gate marked
    auto meter = inner.removeFromBottom (10).toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (meter, 5.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, (db + 70.0f) / 60.0f);
    g.setColour (note >= 0 ? accent : dim);
    g.fillRoundedRectangle (meter.withWidth (juce::jmax (0.0f, meter.getWidth() * lvl)), 5.0f);
    const float gateDb = *processor.apvts.getRawParameterValue ("gate");
    const float gx = meter.getX() + meter.getWidth() * juce::jlimit (0.0f, 1.0f, (gateDb + 70.0f) / 60.0f);
    g.setColour (juce::Colours::white);
    g.fillRect (gx - 1.0f, meter.getY() - 3.0f, 2.0f, meter.getHeight() + 6.0f);
}

void HumToMidiEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (displayHeight);
    area.reduce (14, 4);
    for (auto& r : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        r->label.setBounds (line.removeFromLeft (130));
        r->slider.setBounds (line);
    }
    muteButton.setBounds (area.removeFromTop (28));
}

void HumToMidiEditor::timerCallback()
{
    const int n = processor.displayNote.load();
    const float hz = processor.displayHz.load();
    const float db = processor.displayLevelDb.load();
    if (n != shownNote || std::fabs (hz - shownHz) > 0.2f || std::fabs (db - shownLevel) > 0.5f)
    {
        shownNote = n;
        shownHz = hz;
        shownLevel = db;
        repaint (0, 0, getWidth(), displayHeight);
    }
}
