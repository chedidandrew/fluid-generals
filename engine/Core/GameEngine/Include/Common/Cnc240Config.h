// C&C 240 local engine changes, 2026.
// GPL-3.0-or-later with the additional EA terms in the repository LICENSE.md.
#pragma once

namespace cnc240
{
    enum { MinimumRenderFps = 30, MaximumRenderFps = 240, DefaultRenderFps = 240 };
    constexpr float UserCameraMaximumHeight = 900.0f;
    constexpr float MouseWheelZoomSpeedMultiplier = 3.0f;
    constexpr float MouseEdgeScrollSpeedMultiplier = 3.0f;

    inline int clampRenderFps(int fps)
    {
        return fps < MinimumRenderFps ? MinimumRenderFps :
               fps > MaximumRenderFps ? MaximumRenderFps : fps;
    }
}
