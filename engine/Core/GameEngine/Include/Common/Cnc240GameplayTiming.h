// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include "Common/Cnc240SimulationRate.h"
#include <cstring>
namespace cnc240 {
// Preserve authored combat intervals and apply firing bonuses before timestep conversion.
// Native simulation still updates movement, collisions and projectiles each tick.
inline bool dragonFlameWeapon(const char *name) {
    return !std::strcmp(name,"DragonTankFlameWeapon") || !std::strcmp(name,"DragonTankFlameWeaponUpgraded")
        || !std::strcmp(name,"DragonTankFireWallWeapon") || !std::strcmp(name,"DragonTankFireWallWeaponUpgraded")
        || !std::strcmp(name,"FireWallSegmentWeapon") || !std::strcmp(name,"FireWallSegmentUpgradedWeapon");
}
inline int combatDuration(float milliseconds) {
    return legacyFrames(static_cast<int>(std::ceil(milliseconds * 0.03f)));
}
inline int combatBonusDuration(int nativeFrames, float rateOfFire) {
    const int authoredFrames=static_cast<int>(std::lround(nativeFrames * authoredStep()));
    return legacyFrames(static_cast<int>(std::floor(authoredFrames / rateOfFire)));
}
// Compatibility names used by the retained Fire Wall sweep implementation.
inline int dragonDuration(float milliseconds) { return combatDuration(milliseconds); }
inline int dragonBonusDuration(int nativeFrames,float bonus) { return combatBonusDuration(nativeFrames,bonus); }
inline int authoredFramesFromNative(int frames) { return static_cast<int>(std::lround(frames*authoredStep())); }
inline float authoredImpactSpeed(float nativeSpeed) { return nativeSpeed/authoredStep(); }
// Damage-state transitions preserve their authored cadence while aiming,
// movement and collision detection continue on every native simulation tick.
inline bool authoredCombatBoundary(unsigned frame) {
    return simulationHz()==30 || frame==0 || (static_cast<unsigned long long>(frame)*30/simulationHz())
        !=(static_cast<unsigned long long>(frame-1)*30/simulationHz());
}
inline int strictCombatDuration(float nativeFrames) {
    return legacyFrames(static_cast<int>(std::floor(nativeFrames*authoredStep()))+1);
}
// Original caret: eight frames per phase at 30 FPS. UI time must keep advancing
// while simulation is paused and must not depend on how often a widget is drawn.
inline bool textEntryCaretVisible(unsigned long long milliseconds) {
    return ((milliseconds * 30 / 1000 / 8) & 1) != 0;
}
}
