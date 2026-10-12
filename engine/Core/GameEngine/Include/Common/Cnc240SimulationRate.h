// Experimental C&C variable-rate simulation, 2026.
// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include <cstdlib>
#include <cmath>
namespace cnc240
{
    // Chosen before module data is constructed. Never change a loaded world's units.
    inline int simulationHz()
    {
        static const int rate = []() {
            const char *text = std::getenv("CNC240_SIM_HZ");
            if (!text) return 120;
            char *end = nullptr;
            const long parsed = std::strtol(text, &end, 10);
            return end != text && *end == '\0' && parsed >= 30 && parsed <= 240 ? static_cast<int>(parsed) : 120;
        }();
        return rate;
    }
    // Authored one-time impulses and angular rates use distance/radians per 30 Hz tick.
    // Native locomotor acceleration already uses dt squared and must not use this helper.
    inline float authoredStep() { return 30.0f / simulationHz(); }
    inline float authoredRetention(float factor) {
        return simulationHz()==30 ? factor : std::pow(factor, authoredStep());
    }
    inline int legacyFrames(int frames)
    {
        return static_cast<int>(std::ceil(frames * (simulationHz() / 30.0)));
    }
}
