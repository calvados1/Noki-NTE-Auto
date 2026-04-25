#include <windows.h>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace fs = std::filesystem;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// ── Color palette (dark theme) ────────────────────────────────────────────
static const COLORREF CLR_BG      = RGB(18,  18,  30);   // deep navy
static const COLORREF CLR_ACCENT  = RGB(88,  166, 255);  // sky blue
static const COLORREF CLR_TITLE   = RGB(235, 235, 255);  // near-white
static const COLORREF CLR_STATUS  = RGB(120, 128, 160);  // muted gray-blue
static const COLORREF CLR_DIVIDER = RGB(42,  44,  68);   // subtle separator
static const COLORREF CLR_PB_BG   = RGB(38,  40,  62);   // progress track

// ── Globals ───────────────────────────────────────────────────────────────
HWND   g_hWnd         = NULL;
HWND   g_hTitle       = NULL;
HWND   g_hBadge       = NULL;
HWND   g_hStaticText  = NULL;
HWND   g_hProgressBar = NULL;
HBRUSH g_hBgBrush     = NULL;
HFONT  g_hTitleFont   = NULL;
HFONT  g_hBadgeFont   = NULL;
HFONT  g_hStatusFont  = NULL;

const wchar_t* g_windowClass = L"NokiLauncherWindow";

void ShowErrorMessage(const std::wstring& message);
void checkAndLaunch(HWND hwnd);
void HandleErrorMessage(HWND hwnd, LPARAM lParam);
BOOL RegisterWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance);
void UpdateProgress(int percentage, const std::wstring& message);
bool IsRunningAsAdmin();
bool RestartAsAdmin();

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        if (!CheckTokenMembership(NULL, adminGroup, &isAdmin))
            isAdmin = FALSE;
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool RestartAsAdmin() {
    wchar_t currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = currentPath;
    sei.hwnd   = NULL;
    sei.nShow  = SW_NORMAL;

    return ShellExecuteExW(&sei) == TRUE;
}

void ShowErrorMessage(const std::wstring& message) {
    MessageBoxW(NULL, message.c_str(), L"启动器错误", MB_ICONERROR | MB_OK);
}

void UpdateProgress(int percentage, const std::wstring& message) {
    if (g_hStaticText)
        SetWindowTextW(g_hStaticText, message.c_str());
    if (g_hProgressBar)
        SendMessage(g_hProgressBar, PBM_SETPOS, percentage, 0);
}

void checkAndLaunch(HWND hwnd) {
    wchar_t current_path[MAX_PATH];
    GetModuleFileNameW(NULL, current_path, MAX_PATH);
    fs::path current_dir = fs::path(current_path).parent_path();
    fs::path exe_path    = current_dir / "dist" / "Noki_HBR_Auto.exe";

    try {
        UpdateProgress(10, L"正在初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        UpdateProgress(30, L"检查目标文件...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (fs::exists(exe_path)) {
            UpdateProgress(60, L"正在启动目标程序...");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            std::wstring exe_str = exe_path.wstring();
            wchar_t* cmd_line    = &exe_str[0];

            if (CreateProcessW(NULL, cmd_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                UpdateProgress(100, L"启动成功！");
                std::this_thread::sleep_for(std::chrono::milliseconds(800));

                PostMessage(hwnd, WM_CLOSE, 0, 0);
            } else {
                DWORD error = GetLastError();
                std::wstring msg = L"无法启动程序: " + exe_path.wstring() +
                                   L"\n错误代码: " + std::to_wstring(error);
                PostMessage(hwnd, WM_USER + 1, 0, (LPARAM)new std::wstring(msg));
            }
        } else {
            std::wstring msg = L"目标程序不存在:\n" + exe_path.wstring();
            PostMessage(hwnd, WM_USER + 1, 0, (LPARAM)new std::wstring(msg));
        }
    } catch (const std::exception& e) {
        std::string s = e.what();
        std::wstring msg = L"发生异常: " + std::wstring(s.begin(), s.end());
        PostMessage(hwnd, WM_USER + 1, 0, (LPARAM)new std::wstring(msg));
    }
}

void HandleErrorMessage(HWND hwnd, LPARAM lParam) {
    std::wstring* msg = (std::wstring*)lParam;
    ShowErrorMessage(*msg);
    delete msg;
    PostMessage(hwnd, WM_CLOSE, 0, 0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_CREATE: {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

        // ── Title "Noki 启动器" ───────────────────────────────────────────
        g_hTitle = CreateWindowW(L"STATIC", L"Noki 启动器",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            24, 20, 300, 34,
            hwnd, NULL, hInst, NULL);
        g_hTitleFont = CreateFontW(
            26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        SendMessage(g_hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

        // ── Badge "管理员模式" (right-aligned, accent color) ─────────────
        g_hBadge = CreateWindowW(L"STATIC", L"管理员模式",
            WS_VISIBLE | WS_CHILD | SS_RIGHT,
            336, 32, 120, 17,
            hwnd, NULL, hInst, NULL);
        g_hBadgeFont = CreateFontW(
            13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        SendMessage(g_hBadge, WM_SETFONT, (WPARAM)g_hBadgeFont, TRUE);

        // ── Status text ──────────────────────────────────────────────────
        g_hStaticText = CreateWindowW(L"STATIC", L"正在初始化...",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            24, 78, 432, 22,
            hwnd, NULL, hInst, NULL);
        g_hStatusFont = CreateFontW(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        SendMessage(g_hStaticText, WM_SETFONT, (WPARAM)g_hStatusFont, TRUE);

        // ── Progress bar (thin, custom colored) ─────────────────────────
        g_hProgressBar = CreateWindowW(PROGRESS_CLASSW, NULL,
            WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
            24, 112, 432, 8,
            hwnd, NULL, hInst, NULL);
        // Disable visual styles so PBM_SETBARCOLOR takes effect
        SetWindowTheme(g_hProgressBar, L"", L"");
        SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(g_hProgressBar, PBM_SETPOS, 0, 0);
        SendMessage(g_hProgressBar, PBM_SETBARCOLOR, 0, CLR_ACCENT);
        SendMessage(g_hProgressBar, PBM_SETBKCOLOR, 0, CLR_PB_BG);

        std::thread(checkAndLaunch, hwnd).detach();
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Accent strip at very top (4px)
        RECT rcStrip = { 0, 0, rcClient.right, 4 };
        HBRUSH hAccentBrush = CreateSolidBrush(CLR_ACCENT);
        FillRect(hdc, &rcStrip, hAccentBrush);
        DeleteObject(hAccentBrush);

        // Thin horizontal divider below title area
        HPEN hPen    = CreatePen(PS_SOLID, 1, CLR_DIVIDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 24, 64, NULL);
        LineTo(hdc, rcClient.right - 24, 64);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC   hdc   = (HDC)wParam;
        HWND  hCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        if (hCtrl == g_hTitle)
            SetTextColor(hdc, CLR_TITLE);
        else if (hCtrl == g_hBadge)
            SetTextColor(hdc, CLR_ACCENT);
        else
            SetTextColor(hdc, CLR_STATUS);
        return (LRESULT)g_hBgBrush;
    }

    case WM_DESTROY:
        if (g_hBgBrush)    { DeleteObject(g_hBgBrush);    g_hBgBrush    = NULL; }
        if (g_hTitleFont)  { DeleteObject(g_hTitleFont);  g_hTitleFont  = NULL; }
        if (g_hBadgeFont)  { DeleteObject(g_hBadgeFont);  g_hBadgeFont  = NULL; }
        if (g_hStatusFont) { DeleteObject(g_hStatusFont); g_hStatusFont = NULL; }
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_USER + 1:
        HandleErrorMessage(hwnd, lParam);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

BOOL RegisterWindowClass(HINSTANCE hInstance) {
    g_hBgBrush = CreateSolidBrush(CLR_BG);

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = g_windowClass;
    wc.hIcon         = LoadIcon(hInstance, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBgBrush;
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    return RegisterClassW(&wc);
}

HWND CreateMainWindow(HINSTANCE hInstance) {
    return CreateWindowExW(
        0,
        g_windowClass,
        L"Noki 启动器",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 190,
        NULL, NULL, hInstance, NULL
    );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!IsRunningAsAdmin()) {
        if (RestartAsAdmin())
            return 0;
        ShowErrorMessage(L"需要管理员权限才能运行此程序。\n请右键点击程序，选择"以管理员身份运行"。");
        return 1;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    if (!RegisterWindowClass(hInstance)) {
        ShowErrorMessage(L"无法注册窗口类");
        return 1;
    }

    g_hWnd = CreateMainWindow(hInstance);
    if (!g_hWnd) {
        ShowErrorMessage(L"无法创建窗口");
        return 1;
    }

    // Dark title bar (Windows 10 20H1+)
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    // Center on screen
    RECT rc;
    GetWindowRect(g_hWnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(g_hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
