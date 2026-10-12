// GPL-3.0-or-later with additional EA terms in LICENSE.md.
#pragma once
#include "Common/INI.h"
#include "Common/Cnc240GameplayTiming.h"
namespace cnc240 {
inline void parseCombatDuration(INI *ini,void *instance,void *store,const void *data) {
    if(simulationHz()==30) {INI::parseDurationUnsignedInt(ini,instance,store,data);return;}
    *static_cast<UnsignedInt *>(store)=combatDuration(static_cast<Real>(INI::scanUnsignedInt(ini->getNextToken())));
}
inline void parseCombatPulse(INI *ini,void *instance,void *store,const void *data) {
    if(simulationHz()==30) {INI::parseDurationReal(ini,instance,store,data);return;}
    *static_cast<Real *>(store)=combatDuration(INI::scanReal(ini->getNextToken()));
}
}
