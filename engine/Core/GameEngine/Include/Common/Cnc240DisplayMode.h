// Adaptive display modes, 2026. GPL-3.0-or-later with EA terms.
#pragma once
#include <windows.h>
#include "Common/Cnc240TextClock.h"
namespace cnc240 {
    enum DisplayMode { Fullscreen=0, Borderless=1, Windowed=2 };
    inline int &displayMode() { static int mode=Fullscreen; return mode; }
    inline const wchar_t *displayModeLabel(int mode) {
        return mode==Borderless ? L"Borderless" : mode==Windowed ? L"Windowed" : L"Fullscreen";
    }
    inline void applyWindowMode(HWND window, int width, int height) {
        if (!window) return;
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi);
        DWORD style=WS_POPUP | WS_VISIBLE | WS_SYSMENU;
        if (displayMode()==Windowed) style |= WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
        SetWindowLong(window, GWL_STYLE, style);
        if (displayMode()==Borderless) {
            SetWindowPos(window, (cnc240::boundedInputIsolated()?HWND_BOTTOM:HWND_NOTOPMOST), mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
        } else if (displayMode()==Windowed) {
            RECT r={0,0,width,height}; AdjustWindowRect(&r, style, FALSE);
            // Keep the complete framed window on the work area even when rendering
            // at desktop resolution. Mouse mapping already scales the client area.
            int w=min(r.right-r.left, mi.rcWork.right-mi.rcWork.left);
            int h=min(r.bottom-r.top, mi.rcWork.bottom-mi.rcWork.top);
            SetWindowPos(window, (cnc240::boundedInputIsolated()?HWND_BOTTOM:HWND_NOTOPMOST),
                max(mi.rcWork.left, (mi.rcWork.left+mi.rcWork.right-w)/2),
                max(mi.rcWork.top, (mi.rcWork.top+mi.rcWork.bottom-h)/2), w,h,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
        } else {
            SetWindowPos(window, (cnc240::boundedInputIsolated()?HWND_BOTTOM:HWND_TOPMOST), mi.rcMonitor.left, mi.rcMonitor.top, width,height,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
    }
    inline int mapClientAxis(int position, int renderExtent, int clientExtent) {
        if (renderExtent <= 1 || clientExtent <= 1) return 0;
        // Map both endpoints exactly: an upscaled render must still reach its
        // final pixel when the pointer reaches the final physical client pixel.
        return max(0,min(renderExtent-1,MulDiv(position,renderExtent-1,clientExtent-1)));
    }
    inline void mapClientPoint(HWND window, int width, int height, int &x, int &y) {
        RECT r={}; GetClientRect(window,&r);
        if (r.right>0 && r.bottom>0) {
            x=mapClientAxis(x,width,r.right);
            y=mapClientAxis(y,height,r.bottom);
        }
    }
}
