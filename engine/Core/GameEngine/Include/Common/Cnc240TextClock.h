// Authored text timing and bounded diagnostic input isolation.
// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include "Common/Cnc240SimulationRate.h"

namespace cnc240 {
inline unsigned int authoredTextFrame(unsigned int nativeFrame) {
    return static_cast<unsigned int>(static_cast<unsigned long long>(nativeFrame) * 30 / simulationHz());
}

class AuthoredTextClock {
public:
    AuthoredTextClock() : m_nativeFrame(0), m_frame(0), m_fraction(0) {}
    void reset(unsigned int nativeFrame) {
        m_nativeFrame = nativeFrame;
        m_frame = authoredTextFrame(nativeFrame);
        m_fraction = 0;
    }
    int advance(unsigned int nativeFrame, double frozenSeconds = 0) {
        if (nativeFrame < m_nativeFrame) reset(nativeFrame);
        const unsigned int previousNativeFrame = m_nativeFrame;
        m_fraction += (nativeFrame - m_nativeFrame) * (30.0 / simulationHz());
        m_nativeFrame = nativeFrame;
        if (nativeFrame == previousNativeFrame) m_fraction += frozenSeconds * 30.0;
        const int ticks = static_cast<int>(std::floor(m_fraction + 0.000001));
        m_fraction -= ticks;
        return ticks;
    }
    unsigned int nextFrame() { return ++m_frame; }
    unsigned int frame() const { return m_frame; }
private:
    unsigned int m_nativeFrame, m_frame;
    double m_fraction;
};

inline bool boundedInputIsolated() {
    const char *isolated = std::getenv("CNC240_VERIFY_INPUT_ISOLATED");
    const char *duration = std::getenv("CNC240_VERIFY_SECONDS");
    if (!isolated || isolated[0] != '1' || isolated[1] || !duration) return false;
    char *end = nullptr;
    const long seconds = std::strtol(duration, &end, 10);
    return end != duration && !*end && seconds >= 1 && seconds <= 120;
}
}
