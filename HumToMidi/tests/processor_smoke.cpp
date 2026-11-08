// Optional smoke test: pushes a synthetic hum through the real plugin processor and checks the MIDI it emits.
// Build with -DHUM_BUILD_SMOKE_TEST=ON
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 480; // not a divisor of the analysis hop on purpose

    HumToMidiProcessor p;
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);

    struct Ev { bool on; int note, ch, vel; double ms; };
    std::vector<Ev> events;
    bool mutedOutputSilent = true;

    double phase = 0.0;
    const int totalBlocks = (int) (2.0 * sr / block);
    for (int b = 0; b < totalBlocks; ++b)
    {
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < block; ++i)
        {
            const double t = ((double) b * block + i) / sr;
            const bool hum = t > 0.3 && t < 1.3; // 1 s of A3
            phase += 6.283185307179586 * 220.0 / sr;
            const float v = hum ? 0.1f * (float) (std::sin (phase) + 0.5 * std::sin (2 * phase)) : 0.0f;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v * 0.8f); // slightly different right channel
        }

        juce::MidiBuffer midi;
        p.processBlock (buf, midi);
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOnOrOff())
                events.push_back ({ msg.isNoteOn(), msg.getNoteNumber(), msg.getChannel(), msg.getVelocity(),
                                    1000.0 * ((double) b * block + m.samplePosition) / sr });
        }
    }

    // mute option
    if (auto* mute = p.apvts.getParameter ("mute"))
        mute->setValueNotifyingHost (1.0f);
    juce::AudioBuffer<float> buf (2, block);
    for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.3f); buf.setSample (1, i, 0.3f); }
    juce::MidiBuffer midi;
    p.processBlock (buf, midi);
    for (int i = 0; i < block; ++i)
        if (buf.getSample (0, i) != 0.0f) mutedOutputSilent = false;

    int fails = 0;
    for (auto& e : events)
        std::printf ("  %s note %d ch %d vel %d @ %.0f ms\n", e.on ? "on " : "off", e.note, e.ch, e.vel, e.ms);

    auto check = [&] (bool ok, const char* what) { std::printf ("  %s  %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; };
    check (events.size() == 2, "exactly one note on + one note off");
    if (events.size() == 2)
    {
        check (events[0].on && events[0].note == 57 && events[0].ch == 1, "note on = A3 (57) on channel 1");
        check (! events[1].on && events[1].note == 57, "note off = A3");
        check (events[0].ms > 300.0 && events[0].ms < 520.0, "note on lands within ~200 ms of the hum starting");
    }
    check (mutedOutputSilent, "mute option silences the audio output");
    return fails ? 1 : 0;
}
