// PreviewHost: windowed dev harness for RetroSavers.
//
//   PreviewHost.exe [path\to\Saver.scr]
//
// Hosts "<saver> /p <hwnd>" inside a resizable window (the same protocol the Windows
// Screen Saver Settings preview uses), so a saver can be developed without going
// fullscreen. Buttons: Configure (runs "/c:<hwnd>"), Restart, Open.
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr int kToolbarHeight = 36;
constexpr int IDC_CONFIGURE = 101;
constexpr int IDC_RESTART = 102;
constexpr int IDC_OPEN = 103;
constexpr int IDC_FULLSCREEN = 104;

HWND g_main = nullptr;
HWND g_panel = nullptr;      // the saver's parent: its preview window is created as a child of this
HWND g_buttons[4]{};
std::wstring g_saverPath;
PROCESS_INFORMATION g_child{};
HANDLE g_job = nullptr;      // kill-on-close job: the preview dies with the host even if the host is terminated

int Scale(HWND hwnd, int v) { return MulDiv(v, GetDpiForWindow(hwnd), 96); }

void KillChild() {
    if (g_child.hProcess) {
        if (WaitForSingleObject(g_child.hProcess, 0) == WAIT_TIMEOUT) TerminateProcess(g_child.hProcess, 0);
        CloseHandle(g_child.hProcess);
        CloseHandle(g_child.hThread);
        g_child = {};
    }
}

bool Launch(const std::wstring& args, PROCESS_INFORMATION* out) {
    if (g_saverPath.empty()) return false;
    std::wstring cmd = L"\"" + g_saverPath + L"\" " + args;
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxW(g_main, (L"Could not start:\n" + cmd).c_str(), L"PreviewHost", MB_ICONERROR);
        return false;
    }
    if (g_job) AssignProcessToJobObject(g_job, pi.hProcess);
    if (out) *out = pi;
    else { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return true;
}

void FitChild() {
    HWND child = GetWindow(g_panel, GW_CHILD);
    if (!child) return;
    RECT rc;
    GetClientRect(g_panel, &rc);
    SetWindowPos(child, nullptr, 0, 0, rc.right, rc.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
}

void StartPreview() {
    KillChild();
    wchar_t buf[64];
    swprintf_s(buf, L"/p %llu", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_panel)));
    Launch(buf, &g_child);
    SetTimer(g_main, 1, 100, nullptr);   // fit the child once it has created its window
}

void UpdateTitle() {
    std::wstring title = L"RetroSavers PreviewHost";
    if (!g_saverPath.empty()) {
        size_t slash = g_saverPath.find_last_of(L"\\/");
        title += L" - " + g_saverPath.substr(slash == std::wstring::npos ? 0 : slash + 1);
    }
    SetWindowTextW(g_main, title.c_str());
}

void OpenSaver() {
    wchar_t buf[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"Screen savers (*.scr;*.exe)\0*.scr;*.exe\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        g_saverPath = buf;
        UpdateTitle();
        StartPreview();
    }
}

void Layout() {
    RECT rc;
    GetClientRect(g_main, &rc);
    int tb = Scale(g_main, kToolbarHeight);
    int x = Scale(g_main, 8), w = Scale(g_main, 90), h = Scale(g_main, 24), gap = Scale(g_main, 6);
    for (HWND b : g_buttons) {
        SetWindowPos(b, nullptr, x, (tb - h) / 2, w, h, SWP_NOZORDER);
        x += w + gap;
    }
    SetWindowPos(g_panel, nullptr, 0, tb, rc.right, rc.bottom - tb, SWP_NOZORDER);
    FitChild();
}

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        FitChild();
        return 0;
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        return 1;
    }
    case WM_PARENTNOTIFY:
        if (LOWORD(wParam) == WM_CREATE) PostMessageW(g_main, WM_APP, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE inst = GetModuleHandleW(nullptr);
        const wchar_t* labels[] = { L"&Configure", L"&Restart", L"&Open...", L"&Fullscreen" };
        const int ids[] = { IDC_CONFIGURE, IDC_RESTART, IDC_OPEN, IDC_FULLSCREEN };
        for (int i = 0; i < 4; ++i)
            g_buttons[i] = CreateWindowW(L"BUTTON", labels[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(ids[i])), inst, nullptr);
        g_panel = CreateWindowW(L"RetroSaversPreviewPanel", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        NONCLIENTMETRICSW ncm{ sizeof(ncm) };
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, GetDpiForWindow(hwnd)))
            font = CreateFontIndirectW(&ncm.lfMessageFont);
        for (HWND b : g_buttons) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    }
    case WM_SIZE:
        Layout();
        return 0;
    case WM_DPICHANGED: {
        const RECT* rc = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_APP:   // child created its preview window
    case WM_TIMER:
        FitChild();
        if (msg == WM_TIMER && GetWindow(g_panel, GW_CHILD)) KillTimer(hwnd, 1);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CONFIGURE: {
            wchar_t buf[64];
            swprintf_s(buf, L"/c:%llu", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
            Launch(buf, nullptr);
            return 0;
        }
        case IDC_RESTART:
            StartPreview();
            return 0;
        case IDC_OPEN:
            OpenSaver();
            return 0;
        case IDC_FULLSCREEN:
            Launch(L"/s", nullptr);
            return 0;
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_F5) StartPreview();
        return 0;
    case WM_DESTROY:
        KillChild();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_job = CreateJobObjectW(nullptr, nullptr);
    if (g_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        wchar_t full[MAX_PATH]{};
        if (GetFullPathNameW(argv[1], MAX_PATH, full, nullptr)) g_saverPath = full;
    }
    if (argv) LocalFree(argv);

    WNDCLASSW pc{};
    pc.lpfnWndProc = PanelProc;
    pc.hInstance = inst;
    pc.lpszClassName = L"RetroSaversPreviewPanel";
    pc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&pc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"RetroSaversPreviewHost";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    g_main = CreateWindowW(L"RetroSaversPreviewHost", L"RetroSavers PreviewHost", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1024, 640, nullptr, nullptr, inst, nullptr);
    UpdateTitle();
    ShowWindow(g_main, show);
    if (!g_saverPath.empty()) StartPreview();
    else OpenSaver();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    KillChild();
    return 0;
}
