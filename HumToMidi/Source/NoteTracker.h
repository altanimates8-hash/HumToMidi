#pragma once
// Turns a stream of per-frame pitch estimates into clean MIDI note on/off events.
#include <algorithm>
#include <cmath>

namespace hum
{
struct TrackerSettings
{
    float gateDb    = -48.0f; // input level needed to count as "humming"
    float minNoteMs = 60.0f;  // pitch must be steady this long before a note starts
    float switchMs  = 70.0f;  // a new pitch must hold this long before the note changes
    float releaseMs = 90.0f;  // silence/unpitched this long ends the note
    int   transpose = 0;      // semitones
    int   velocity  = 0;      // 0 = follow volume, 1..127 = fixed
};

struct NoteEvent
{
    bool on = false;
    int  note = 0;
    int  velocity = 0;
};

struct TrackerEvents
{
    NoteEvent items[2];
    int count = 0;
    void add (bool on, int note, int velocity) { items[count++] = { on, note, velocity }; }
};

class NoteTracker
{
public:
    void prepare (float hopMilliseconds)
    {
        hopMs = hopMilliseconds;
        reset();
    }

    void reset()
    {
        rawNote = outNote = candidate = -1;
        candCount = unvoicedCount = histCount = histPos = 0;
        startPeakDb = -120.0f;
    }

    int activeNote() const { return outNote; }

    // If a note is sounding, ends it and returns true (used for panic / bypass).
    bool forceOff (NoteEvent& e)
    {
        if (outNote < 0)
            return false;
        e = { false, outNote, 0 };
        rawNote = outNote = candidate = -1;
        candCount = unvoicedCount = histCount = 0;
        return true;
    }

    TrackerEvents process (bool pitched, float midiFloat, float levelDb, const TrackerSettings& s)
    {
        TrackerEvents out;
        const float gate = outNote >= 0 ? s.gateDb - 6.0f : s.gateDb; // gate hysteresis
        const bool voiced = pitched && levelDb >= gate;

        if (! voiced)
        {
            candidate = -1;
            candCount = 0;
            histCount = 0;
            if (outNote >= 0)
            {
                if (++unvoicedCount >= frames (s.releaseMs))
                {
                    out.add (false, outNote, 0);
                    rawNote = outNote = -1;
                    unvoicedCount = 0;
                }
            }
            else
                unvoicedCount = 0;
            return out;
        }

        unvoicedCount = 0;

        // 5-frame median removes single-frame octave/glitch errors
        hist[histPos] = midiFloat;
        histPos = (histPos + 1) % kHist;
        histCount = std::min (histCount + 1, kHist);
        float tmp[kHist];
        for (int i = 0; i < histCount; ++i)
        {
            float v = hist[i];
            int j = i;
            while (j > 0 && tmp[j - 1] > v) { tmp[j] = tmp[j - 1]; --j; }
            tmp[j] = v;
        }
        const float m = tmp[histCount / 2];
        const int target = (int) std::lround (m);

        if (outNote < 0)
        {
            if (target == candidate)
                ++candCount;
            else
            {
                candidate = target;
                candCount = 1;
                startPeakDb = levelDb;
            }
            startPeakDb = std::max (startPeakDb, levelDb);

            if (candCount >= frames (s.minNoteMs))
                startNote (target, startPeakDb, s, out);
        }
        else
        {
            // stay on the current note while the pitch drifts within its zone (vibrato etc.)
            if (std::fabs (m - (float) rawNote) <= 0.5f + kHysteresis)
            {
                candidate = -1;
                candCount = 0;
                return out;
            }

            if (target == candidate)
                ++candCount;
            else
            {
                candidate = target;
                candCount = 1;
            }

            if (candCount >= frames (s.switchMs))
            {
                out.add (false, outNote, 0);
                startNote (target, levelDb, s, out);
            }
        }
        return out;
    }

private:
    static constexpr int   kHist = 5;
    static constexpr float kHysteresis = 0.2f;

    int frames (float ms) const { return std::max (1, (int) std::lround (ms / hopMs)); }

    void startNote (int target, float levelDb, const TrackerSettings& s, TrackerEvents& out)
    {
        rawNote = target;
        outNote = std::clamp (target + s.transpose, 0, 127);
        int vel = s.velocity;
        if (vel <= 0)
            vel = std::clamp ((int) std::lround (30.0f + (levelDb + 50.0f) * (97.0f / 38.0f)), 1, 127);
        out.add (true, outNote, vel);
        candidate = -1;
        candCount = 0;
    }

    float hopMs = 6.0f;
    int rawNote = -1, outNote = -1, candidate = -1, candCount = 0, unvoicedCount = 0;
    float hist[kHist] = {};
    int histCount = 0, histPos = 0;
    float startPeakDb = -120.0f;
};
} // namespace hum
