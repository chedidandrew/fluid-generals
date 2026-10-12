// C&C 240 local engine changes, 2026.
// GPL-3.0-or-later with the additional EA terms in LICENSE.md.
#pragma once
namespace cnc240
{
    // Count only successful Direct3D Present calls, as tracked by the renderer.
    unsigned long getPresentedFrameCount();
    bool checkTerrainCoverage();
    bool verifyMinimapDrag();
    bool verifyCameraSpeed();
    void verifyAdaptiveCameraClocks();
    void verifyAdaptiveDisplaySettings();
}
