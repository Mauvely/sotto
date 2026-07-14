#pragma once

#include <algorithm>
#include <cstdint>

// Energy-based voice activity gate with an adaptive noise floor.
// Fed short-window RMS observations on the audio sample clock; reports
// whether speech is currently present. Pure logic, unit-testable.
class SpeechGate
{
public:
    struct Config {
        double absThreshold = 0.006; // linear RMS below which it is never speech
        double noiseFactor = 3.0;    // speech must exceed noiseFloor * factor
        int64_t hangoverMs = 240;    // keep "voice" asserted this long after the last hit
    };

    SpeechGate() = default;
    explicit SpeechGate(Config cfg)
        : m_cfg(cfg)
    {
    }

    // rms: linear RMS of the observed window; nowMs: sample-clock timestamp
    // of the window end. Returns true while voice is considered active.
    bool update(double rms, int64_t nowMs)
    {
        // Asymmetric EMA: the floor falls quickly but rises slowly, so
        // sustained speech doesn't drag it upward.
        if (m_noise <= 0.0)
            m_noise = std::max(rms, 1e-4);
        const double alpha = rms < m_noise ? 0.2 : 0.005;
        m_noise += alpha * (rms - m_noise);

        const double threshold = std::max(m_cfg.absThreshold, m_noise * m_cfg.noiseFactor);
        if (rms >= threshold)
            m_lastVoiceMs = nowMs;
        return active(nowMs);
    }

    bool active(int64_t nowMs) const
    {
        return m_lastVoiceMs >= 0 && (nowMs - m_lastVoiceMs) <= m_cfg.hangoverMs;
    }

    int64_t lastVoiceMs() const { return m_lastVoiceMs; }
    double noiseFloor() const { return m_noise; }

    void reset() { m_lastVoiceMs = -1; } // keeps the learned noise floor

private:
    Config m_cfg;
    double m_noise = 0.0;
    int64_t m_lastVoiceMs = -1;
};
