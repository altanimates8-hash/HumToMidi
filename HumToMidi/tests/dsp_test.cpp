// Tests for the JUCE-free hum -> MIDI engine.
#include "../Source/HumAnalyzer.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <random>
#include <utility>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (cond) std::printf ("  PASS  ");               \
        else { std::printf ("  FAIL  "); ++failures; }    \
        std::printf (__VA_ARGS__);                        \
        std::printf ("\n");                               \
    } while (0)

struct Seg { double hz; double ms; double vibCents; };

// Vocal-ish hum: a few harmonics, optional vibrato. hz == 0 -> near-silence.
static std::vector<float> synth (double sr, const std::vector<Seg>& segs, float amp = 0.12f)
{
    std::vector<float> out;
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> n (-1.0f, 1.0f);
    double phase = 0.0, t = 0.0;
    const double twoPi = 6.283185307179586;
    for (const auto& s : segs)
    {
        const int len = (int) (s.ms * 0.001 * sr);
        for (int i = 0; i < len; ++i, t += 1.0 / sr)
        {
            if (s.hz <= 0.0) { out.push_back (n (rng) * 1.0e-4f); continue; }
            const double f = s.hz * std::pow (2.0, s.vibCents * std::sin (twoPi * 5.5 * t) / 1200.0);
            phase += twoPi * f / sr;
            const double v = std::sin (phase) + 0.6 * std::sin (2 * phase) + 0.35 * std::sin (3 * phase) + 0.2 * std::sin (4 * phase);
            out.push_back ((float) (v * amp / 2.0) + n (rng) * 0.002f);
        }
    }
    return out;
}

struct Got { bool on; int note; int vel; double ms; };

static std::vector<Got> run (const std::vector<float>& audio, double sr, hum::TrackerSettings s = {})
{
    hum::HumAnalyzer a;
    a.prepare (sr);
    a.settings = s;
    std::vector<Got> got;
    const int block = 512;
    for (size_t pos = 0; pos < audio.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, audio.size() - pos);
        a.process (audio.data() + pos, n, [&] (const hum::HumAnalyzer::Event& e) {
            got.push_back ({ e.on, e.note, e.velocity, 1000.0 * ((double) pos + e.offset) / sr });
        });
    }
    return got;
}

static bool sequenceIs (const std::vector<Got>& g, std::initializer_list<std::pair<bool, int>> expect)
{
    if (g.size() != expect.size()) return false;
    size_t i = 0;
    for (auto& e : expect)
    {
        if (g[i].on != e.first || g[i].note != e.second) return false;
        ++i;
    }
    return true;
}

static void dump (const std::vector<Got>& g)
{
    for (auto& e : g) std::printf ("        %s %d vel %d @ %.0f ms\n", e.on ? "on " : "off", e.note, e.vel, e.ms);
}

int main()
{
    for (double sr : { 44100.0, 48000.0 })
    {
        std::printf ("== sample rate %.0f ==\n", sr);

        std::printf ("detector accuracy\n");
        for (double hz : { 82.4, 110.0, 220.0, 440.0, 880.0 })
        {
            hum::PitchDetector d;
            d.prepare (sr, 65.0f, 1000.0f);
            auto audio = synth (sr, { { hz, 200, 0 } });
            auto r = d.detect (audio.data() + 2000);
            const double cents = r.voiced ? 1200.0 * std::log2 (r.frequency / hz) : 9999.0;
            CHECK (r.voiced && std::fabs (cents) < 10.0, "%.1f Hz -> %.2f Hz (%.1f cents)", hz, r.frequency, cents);
        }

        {
            std::printf ("silence + noise are unpitched\n");
            std::vector<float> noise (44100);
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> n (-0.1f, 0.1f);
            for (auto& v : noise) v = n (rng);
            auto g = run (noise, sr);
            CHECK (g.empty(), "1 s of white noise -> %zu events", g.size());
            g = run (synth (sr, { { 0, 1000, 0 } }), sr);
            CHECK (g.empty(), "1 s of silence -> %zu events", g.size());
        }

        {
            std::printf ("single held note with vibrato\n");
            auto g = run (synth (sr, { { 0, 300, 0 }, { 220.0, 600, 30 }, { 0, 400, 0 } }), sr);
            dump (g);
            CHECK (sequenceIs (g, { { true, 57 }, { false, 57 } }), "A3 (57) on/off, no stray notes");
            if (g.size() >= 1) CHECK (g[0].ms - 300.0 < 130.0, "note-on lag %.0f ms after hum starts", g[0].ms - 300.0);
        }

        {
            std::printf ("pitch change\n");
            auto g = run (synth (sr, { { 0, 200, 0 }, { 220.0, 500, 0 }, { 261.63, 500, 0 }, { 0, 400, 0 } }), sr);
            dump (g);
            CHECK (sequenceIs (g, { { true, 57 }, { false, 57 }, { true, 60 }, { false, 60 } }), "A3 -> C4");
        }

        {
            std::printf ("range\n");
            auto g = run (synth (sr, { { 0, 200, 0 }, { 98.0, 500, 0 }, { 0, 300, 0 }, { 880.0, 500, 0 }, { 0, 300, 0 } }), sr);
            dump (g);
            CHECK (sequenceIs (g, { { true, 43 }, { false, 43 }, { true, 81 }, { false, 81 } }), "G2 (43) then A5 (81)");
        }

        {
            std::printf ("short blip is ignored\n");
            auto g = run (synth (sr, { { 0, 300, 0 }, { 330.0, 25, 0 }, { 0, 400, 0 } }), sr);
            CHECK (g.empty(), "25 ms blip -> %zu events", g.size());
        }

        {
            std::printf ("transpose + fixed velocity\n");
            hum::TrackerSettings s;
            s.transpose = 12;
            s.velocity = 100;
            auto g = run (synth (sr, { { 0, 200, 0 }, { 220.0, 500, 0 }, { 0, 300, 0 } }), sr, s);
            CHECK (g.size() == 2 && g[0].note == 69 && g[0].vel == 100, "A3 +12 -> 69 at velocity 100");
        }

        {
            std::printf ("volume -> velocity\n");
            auto quiet = run (synth (sr, { { 0, 200, 0 }, { 220.0, 500, 0 }, { 0, 300, 0 } }, 0.02f), sr);
            auto loud  = run (synth (sr, { { 0, 200, 0 }, { 220.0, 500, 0 }, { 0, 300, 0 } }, 0.5f), sr);
            CHECK (! quiet.empty() && ! loud.empty() && loud[0].vel > quiet[0].vel + 15, "quiet vel %d < loud vel %d",
                   quiet.empty() ? -1 : quiet[0].vel, loud.empty() ? -1 : loud[0].vel);
        }
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
