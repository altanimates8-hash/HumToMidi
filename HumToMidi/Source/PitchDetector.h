#pragma once
// Monophonic pitch detector (YIN algorithm). No JUCE dependency so it can be unit tested on its own.
#include <algorithm>
#include <cmath>
#include <vector>

namespace hum
{
struct PitchResult
{
    bool  voiced    = false;
    float frequency = 0.0f; // Hz, valid only when voiced
    float clarity   = 0.0f; // 0..1, higher = cleaner periodicity
};

class PitchDetector
{
public:
    void prepare (double sampleRate, float minHz, float maxHz)
    {
        sr     = (float) sampleRate;
        tauMin = std::max (2, (int) std::floor (sampleRate / maxHz));
        tauMax = std::max (tauMin + 2, (int) std::ceil (sampleRate / minHz));
        window = std::max (tauMax, 512);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 1.0f);
    }

    // Number of samples detect() reads.
    int requiredSamples() const { return window + tauMax + 1; }

    PitchResult detect (const float* x)
    {
        PitchResult r;

        // 1. difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float* b = x + tau;
            float sum = 0.0f;
            for (int j = 0; j < window; ++j)
            {
                const float d = x[j] - b[j];
                sum += d * d;
            }
            diff[(size_t) tau] = sum;
        }

        // 2. cumulative mean normalised difference
        cmnd[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 1.0e-12f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
        }
        cmnd[(size_t) tauMax + 1] = 1.0f;

        // 3. first dip below the absolute threshold, then walk down to its local minimum
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < absThreshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        // 4. fallback: global minimum, accepted only if reasonably periodic
        if (best < 0)
        {
            int m = tauMin;
            for (int tau = tauMin + 1; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) m])
                    m = tau;
            if (cmnd[(size_t) m] >= fallbackThreshold)
                return r;
            best = m;
        }

        // 5. parabolic interpolation for sub-sample accuracy
        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cmnd[(size_t) best - 1], s1 = cmnd[(size_t) best], s2 = cmnd[(size_t) best + 1];
            const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
            if (std::fabs (denom) > 1.0e-9f)
                tauF += std::clamp ((s0 - s2) / denom, -1.0f, 1.0f);
        }

        r.voiced    = true;
        r.frequency = sr / tauF;
        r.clarity   = std::clamp (1.0f - cmnd[(size_t) best], 0.0f, 1.0f);
        return r;
    }

private:
    static constexpr float absThreshold      = 0.20f;
    static constexpr float fallbackThreshold = 0.30f;

    float sr = 44100.0f;
    int tauMin = 2, tauMax = 4, window = 512;
    std::vector<float> diff, cmnd;
};
} // namespace hum
