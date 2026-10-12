// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
#include "Common/Cnc240SimulationRate.h"
#include <cstdio>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cnc240 {
    constexpr int DefaultSimulationHz = 120;
    constexpr int SimulationChoices[] = {120, 60, 144, 240, 30};
    inline const wchar_t *simulationChoiceLabel(int index) {
        static const wchar_t *labels[] = {L"120 Hz (Default)", L"Experimental - 60 Hz", L"Experimental - 144 Hz", L"Experimental - 240 Hz", L"30 Hz (Original rate)"};
        return labels[index >= 0 && index < 5 ? index : 0];
    }
    inline bool simulationRestartRequired(int active,int previous,int selected) {return selected!=active && selected!=previous;}
    inline FILE *openSimulationSettings(const wchar_t *mode) {
#ifdef _WIN32
        const wchar_t *path = _wgetenv(L"CNC240_SIM_SETTINGS_FILE");
        return path && *path ? _wfopen(path, mode) : nullptr;
#else
        const char *path = std::getenv("CNC240_SIM_SETTINGS_FILE");
        return path && *path ? std::fopen(path, mode[0] == L'r' ? "r" : "w") : nullptr;
#endif
    }
    inline int nextSimulationHz() {
        FILE *file = openSimulationSettings(L"r");
        int hz = DefaultSimulationHz;
        if (file) { if (std::fscanf(file, "SimulationHz = %d", &hz) != 1) hz = DefaultSimulationHz; std::fclose(file); }
        return hz >= 30 && hz <= 240 ? hz : DefaultSimulationHz;
    }
    // Only next launch reads this file. Never change parsed time units in a loaded world.
    inline bool saveNextSimulationHz(int hz) {
        if (hz < 30 || hz > 240) return false;
#ifdef _WIN32
        const wchar_t *path = _wgetenv(L"CNC240_SIM_SETTINGS_FILE");
        if (!path || !*path) return false;
        std::wstring temp = std::wstring(path) + L".tmp-" + std::to_wstring(GetCurrentProcessId());
        FILE *file = _wfopen(temp.c_str(), L"w");
        if (!file) return false;
        bool ok = std::fprintf(file, "SimulationHz = %d\n", hz) > 0;
        if (std::fclose(file) != 0) ok = false;
        if (ok) ok = MoveFileExW(temp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok) DeleteFileW(temp.c_str());
        return ok;
#else
        FILE *file = openSimulationSettings(L"w");
        if (!file) return false;
        bool ok = std::fprintf(file, "SimulationHz = %d\n", hz) > 0;
        return std::fclose(file) == 0 && ok;
#endif
    }
}
