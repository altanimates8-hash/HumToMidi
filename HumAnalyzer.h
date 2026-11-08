#pragma once
// Glue: mono samples in -> pitch detection every ~6 ms -> note tracker -> MIDI events out.
// Pure C++ (no JUCE) so the exact code that runs in the plugin is also what the tests run.
#include "NoteTracker.h"
#include "PitchDetector.h"
#include <cmath>
#include <vector>

namespace hum
{
class HumAnalyzer
{
public:
    struct Event
    {
        bool on;
        int  note;
        int  velocity;
        int  offset; // sample offset within the buffer passed to process()
    };

    TrackerSettings settings;

    void prepare (double sampleRate)
    {
        detector.prepare (sampleRate, 65.0f, 1000.0f);
        frameLen = detector.requiredSamples();
        ring.assign ((size_t) frameLen, 0.0f);
        frame.assign ((size_t) frameLen, 0.0f);
        hop = std::max (64, (int) std::lround (sampleRate * 0.006));
        tracker.prepare (1000.0f * (float) hop / (float) sampleRate);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = sinceHop = 0;
        lastHz = 0.0f;
        lastLevelDb = -100.0f;
        tracker.reset();
    }

    int   activeNote() const    { return tracker.activeNote(); }
    float lastFrequency() const { return lastHz; }
    float lastLevel() const     { return lastLevelDb; }

    bool releaseActiveNote (Event& e)
    {
        NoteEvent n;
        if (! tracker.forceOff (n))
            return false;
        e = { false, n.note, 0, 0 };
        return true;
    }

    template <typename Fn>
    void process (const float* mono, int numSamples, Fn&& onEvent)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            ring[(size_t) writePos] = mono[i];
            if (++writePos >= frameLen)
                writePos = 0;

            if (++sinceHop < hop)
                continue;
            sinceHop = 0;

            // linearise ring buffer, oldest sample first
            double sumSq = 0.0;
            for (int k = 0; k < frameLen; ++k)
            {
                const float v = ring[(size_t) ((writePos + k) % frameLen)];
                frame[(size_t) k] = v;
                sumSq += (double) v * v;
            }

            lastLevelDb = 20.0f * std::log10 ((float) std::sqrt (sumSq / frameLen) + 1.0e-9f);

            PitchResult p;
            if (lastLevelDb > settings.gateDb - 12.0f) // skip the heavy maths when clearly silent
                p = detector.detect (frame.data());

            lastHz = p.voiced ? p.frequency : 0.0f;
            const float midi = p.voiced ? 69.0f + 12.0f * std::log2 (p.frequency / 440.0f) : 0.0f;

            const TrackerEvents ev = tracker.process (p.voiced, midi, lastLevelDb, settings);
            for (int e = 0; e < ev.count; ++e)
                onEvent (Event { ev.items[e].on, ev.items[e].note, ev.items[e].velocity, i });
        }
    }

private:
    PitchDetector detector;
    NoteTracker tracker;
    std::vector<float> ring, frame;
    int frameLen = 1024, hop = 256, writePos = 0, sinceHop = 0;
    float lastHz = 0.0f, lastLevelDb = -100.0f;
};
} // namespace hum
