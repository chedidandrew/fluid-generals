// Modified community engine. GPL-3.0-or-later plus EA terms in LICENSE.md.
#pragma once
#include <cstdlib>
#include <cstring>
namespace fluid {
// Player builds show no corner counters or clock by default. A troubleshooting
// launch explicitly enables the existing optional overlays; gameplay is unchanged.
inline bool diagnosticsOverlayEnabled() {
    static const bool enabled=[](){const char *value=std::getenv("FLUID_SHOW_DIAGNOSTICS");return value&&std::strcmp(value,"1")==0;}();
    return enabled;
}
}
