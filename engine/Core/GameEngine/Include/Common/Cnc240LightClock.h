// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include "Common/Cnc240SimulationRate.h"
namespace cnc240 {
// Unsigned subtraction also handles the native millisecond clock wrapping.
// The game clock freezes while paused and accumulates every simulation tick.
class LightFadeClock {
    unsigned start=0; double growMs=0,decayMs=0;
public:
    void reset(unsigned growFrames,unsigned decayFrames,unsigned now) {
        start=now;growMs=growFrames*1000.0/simulationHz();decayMs=decayFrames*1000.0/simulationHz();
    }
    double age(unsigned now) const {return static_cast<unsigned>(now-start);}
    float factor(unsigned now) const {
        const double elapsed=age(now);
        if(growMs>0 && elapsed<growMs)return static_cast<float>(elapsed/growMs);
        if(decayMs==0)return 1;
        const double remaining=1-(elapsed-growMs)/decayMs;
        return static_cast<float>(remaining>0?remaining:0);
    }
    bool expired(unsigned now) const {return decayMs>0 && age(now)>=growMs+decayMs;}
};
inline unsigned authoredLightDuration(unsigned milliseconds) {
    return legacyFrames(static_cast<int>(std::ceil(milliseconds*0.03f)));
}
}
