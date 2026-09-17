#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include "Saver.h"
#include "Gfx/Texture.h"

namespace rs {

using SaverFactory = std::function<std::unique_ptr<Saver>()>;

// scrnsave.lib glue. Each saver's ScreenSaverProcW forwards to Proc; Proc owns the
// render thread and passes everything it does not handle to DefScreenSaverProc.
class Host {
public:
    // captureDesktop: grab the screen at WM_CREATE (run mode only) for savers that draw over the desktop.
    static LRESULT Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, const wchar_t* saverName, SaverFactory factory, bool captureDesktop = false);

    // The image grabbed at WM_CREATE when captureDesktop was requested; nullptr otherwise.
    static const Image* DesktopImage();
};

// Call from RegisterDialogClasses and return its result. Registers the comctl32 classes, then
// shows the DLG_SCRNSAVECONFIGURE dialog itself: owner = the HWND from "/c:<hwnd>" when valid,
// otherwise none (scrnsave.lib would use GetForegroundWindow(), which fails when that is a
// shell surface that cannot own a Win32 dialog). Always returns FALSE so the library does not
// show the dialog a second time.
BOOL RunConfigDialog(DLGPROC proc);

// Registers the comctl32 classes the config dialogs use (RunConfigDialog does this for you).
BOOL RegisterCommonDialogClasses();

} // namespace rs
