#define _WIN32_WINNT 0x0500
#include "WinWindow.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include <process.h>

// --- CONSOLE & SYSTEM ---
static HANDLE hConsole = NULL;

void init_console_handles() {
    if (!hConsole) hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void get_console_size(int* width, int* height) {
    init_console_handles();
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        *width = 80;
        *height = 25;
    }
}

long long get_current_time_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10000; // Convert 100ns to ms
}

void sleep_ms(int ms) {
    Sleep(ms);
}

void console_gotoxy(int x, int y) {
    init_console_handles();
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(hConsole, coord);
}

void console_set_color(int color_code) {
    init_console_handles();
    WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default White
    // Mapping generic color codes to Windows Attributes
    // 0=Reset, 1=Green, 2=Red, 3=Yellow, 4=Blue, 5=White, 6=Grey, 7=Orange
    switch (color_code) {
        case 1: attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break; // Green
        case 2: attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;   // Red
        case 3: attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break; // Yellow
        case 4: attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;  // Blue
        case 5: attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // White
        case 6: attr = FOREGROUND_INTENSITY; break; // Grey-ish
        default: attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
    }
    SetConsoleTextAttribute(hConsole, attr);
}

void console_clear() {
    init_console_handles();
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}

// --- GUI WINDOWS (LOCK & TIMER) ---
static HWND g_hLockWindow = NULL;
static HWND g_hTimerWindow = NULL;
static char g_TimerText[32] = { 0 };
static char g_Title[128] = { 0 };
static char g_Subtitle[128] = { 0 };
static HFONT g_hFontBig = NULL;
static HFONT g_hFontSmall = NULL;
static int timer_hidden = 0;
static CRITICAL_SECTION g_cs;
static HANDLE g_hGuiThread = NULL;
static HWND g_hTrayWnd = NULL;
static NOTIFYICONDATA nid;

#define WM_TRAYICON (WM_USER + 100)
#define ID_TRAY_ICON 1001
#define IDM_TOGGLE_CONSOLE 2001
#define IDM_TOGGLE_TIMER 2002

LRESULT CALLBACK LockWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
     switch (msg) {
          case WM_CREATE: {
               HDC hdc = GetDC(hwnd);
               long lfHeight = -MulDiv(48, GetDeviceCaps(hdc, LOGPIXELSY), 72);
               g_hFontBig = CreateFont(lfHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
               lfHeight = -MulDiv(24, GetDeviceCaps(hdc, LOGPIXELSY), 72);
               g_hFontSmall = CreateFont(lfHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
               ReleaseDC(hwnd, hdc);
               return 0;
          }
          case WM_PAINT: {
               PAINTSTRUCT ps;
               HDC hdc = BeginPaint(hwnd, &ps);
               RECT rect;
               GetClientRect(hwnd, &rect);
               HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
               FillRect(hdc, &rect, hBrush);
               DeleteObject(hBrush);
               SetTextColor(hdc, RGB(255, 255, 255));
               SetBkMode(hdc, TRANSPARENT);
               EnterCriticalSection(&g_cs);
               SelectObject(hdc, g_hFontBig);
               RECT rTitle = rect;
               rTitle.bottom = (rect.bottom + rect.top) / 2;
               DrawText(hdc, g_Title, -1, &rTitle, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
               SelectObject(hdc, g_hFontSmall);
               RECT rSub = rect;
               rSub.top = (rect.bottom + rect.top) / 2;
               rSub.top += 10;
               DrawText(hdc, g_Subtitle, -1, &rSub, DT_CENTER | DT_TOP | DT_SINGLELINE);
               LeaveCriticalSection(&g_cs);
               EndPaint(hwnd, &ps);
               return 0;
          }
          case WM_DESTROY:
               if (g_hFontBig) DeleteObject(g_hFontBig);
               if (g_hFontSmall) DeleteObject(g_hFontSmall);
               g_hLockWindow = NULL;
               return 0;
     }
     return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TimerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
     switch (msg) {
          case WM_PAINT: {
               PAINTSTRUCT ps;
               HDC hdc = BeginPaint(hwnd, &ps);
               RECT rect;
               GetClientRect(hwnd, &rect);
               HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
               FillRect(hdc, &rect, hBrush);
               DeleteObject(hBrush);
               FrameRect(hdc, &rect, GetSysColorBrush(COLOR_3DLIGHT));
               SetTextColor(hdc, RGB(0, 255, 0));
               SetBkMode(hdc, TRANSPARENT);
               HFONT hFont = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
               SelectObject(hdc, hFont);
               EnterCriticalSection(&g_cs);
               DrawText(hdc, g_TimerText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
               LeaveCriticalSection(&g_cs);
               DeleteObject(hFont);
               EndPaint(hwnd, &ps);
               return 0;
          }
          case WM_LBUTTONDOWN: {
               ReleaseCapture();
               SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
               return 0;
          }
          case WM_RBUTTONUP: {
               POINT pt;
               GetCursorPos(&pt);
               HMENU hMenu = CreatePopupMenu();
               AppendMenu(hMenu, MF_STRING, 1, "Hide");
               SetForegroundWindow(hwnd);
               TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
               DestroyMenu(hMenu);
               return 0;
          }
          case WM_COMMAND: {
               if (LOWORD(wParam) == 1) { // Hide Option
                    EnterCriticalSection(&g_cs);
                    timer_hidden = 1;
                    LeaveCriticalSection(&g_cs);
                    ShowWindow(hwnd, SW_HIDE);
               }
               return 0;
          }
     }
     return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
     switch (msg) {
          case WM_TRAYICON:
               if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, IDM_TOGGLE_CONSOLE, "Toggle Console");
                    AppendMenu(hMenu, MF_STRING, IDM_TOGGLE_TIMER, "Toggle Timer");
                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
                    DestroyMenu(hMenu);
               }
               else if (lParam == WM_LBUTTONDBLCLK) {
                    toggle_console();
               }
               break;
          case WM_COMMAND:
               if (LOWORD(wParam) == IDM_TOGGLE_CONSOLE) {
                    toggle_console();
               }
               else if (LOWORD(wParam) == IDM_TOGGLE_TIMER) {
                    EnterCriticalSection(&g_cs);
                    timer_hidden = !timer_hidden;
                    int hidden = timer_hidden;
                    LeaveCriticalSection(&g_cs);
                    if (g_hTimerWindow) {
                         ShowWindow(g_hTimerWindow, hidden ? SW_HIDE : SW_SHOWNOACTIVATE);
                    }
               }
               break;
     }
     return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI GuiThreadProc(LPVOID lpParam) {
     WNDCLASS wc = { 0 };
     wc.lpfnWndProc = LockWndProc;
     wc.hInstance = GetModuleHandle(NULL);
     wc.lpszClassName = "AdminEyeLockScreen";
     wc.hCursor = LoadCursor(NULL, IDC_ARROW);
     RegisterClass(&wc);

     WNDCLASS wc2 = { 0 };
     wc2.lpfnWndProc = TimerWndProc;
     wc2.hInstance = GetModuleHandle(NULL);
     wc2.lpszClassName = "AdminEyeTimer";
     wc2.hCursor = LoadCursor(NULL, IDC_ARROW);
     RegisterClass(&wc2);

     WNDCLASS wc3 = { 0 };
     wc3.lpfnWndProc = TrayWndProc;
     wc3.hInstance = GetModuleHandle(NULL);
     wc3.lpszClassName = "AdminEyeTray";
     RegisterClass(&wc3);

     int w = GetSystemMetrics(SM_CXSCREEN);
     int h = GetSystemMetrics(SM_CYSCREEN);
     g_hLockWindow = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "AdminEyeLockScreen", "Lock Screen", WS_POPUP, 0, 0, w, h, NULL, NULL, GetModuleHandle(NULL), NULL);
     
     g_hTimerWindow = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, "AdminEyeTimer", "Timer", WS_POPUP, w - 180, 20, 160, 50, NULL, NULL, GetModuleHandle(NULL), NULL);
     SetLayeredWindowAttributes(g_hTimerWindow, 0, 204, LWA_ALPHA);

     // Create Hidden Tray Window
     g_hTrayWnd = CreateWindowEx(0, "AdminEyeTray", "AdminEye Tray", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

     // Init Tray Icon
     memset(&nid, 0, sizeof(nid));
     nid.cbSize = sizeof(nid);
     nid.hWnd = g_hTrayWnd;
     nid.uID = ID_TRAY_ICON;
     nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
     nid.uCallbackMessage = WM_TRAYICON;
     nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Default App Icon
     strcpy(nid.szTip, "AdminEye Client");
     Shell_NotifyIcon(NIM_ADD, &nid);

     MSG msg;
     while (GetMessage(&msg, NULL, 0, 0)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
     }
     return 0;
}

void init_window_system() {
     init_console_handles();
     SetConsoleTitle("AdminEye Client");
     
     // Hide Cursor
     CONSOLE_CURSOR_INFO cursorInfo;
     GetConsoleCursorInfo(hConsole, &cursorInfo);
     cursorInfo.bVisible = FALSE;
     SetConsoleCursorInfo(hConsole, &cursorInfo);

     InitializeCriticalSection(&g_cs);
     g_hGuiThread = CreateThread(NULL, 0, GuiThreadProc, NULL, 0, NULL);
     
     // Wait for windows to be created
     while (g_hLockWindow == NULL || g_hTimerWindow == NULL) {
          Sleep(10);
     }
}

void set_window_lockdown(int enable, const char* title, const char* subtitle) {
     EnterCriticalSection(&g_cs);
     int changed = 0;
     if (strcmp(g_Title, title ? title : "") != 0) changed = 1;
     if (strcmp(g_Subtitle, subtitle ? subtitle : "") != 0) changed = 1;

     strncpy(g_Title, title ? title : "", sizeof(g_Title) - 1);
     strncpy(g_Subtitle, subtitle ? subtitle : "", sizeof(g_Subtitle) - 1);
     LeaveCriticalSection(&g_cs);

     if (enable) {
          if (g_hLockWindow) {
               if (!IsWindowVisible(g_hLockWindow)) {
                    ShowWindow(g_hLockWindow, SW_SHOW);
                    ShowCursor(FALSE);
                    changed = 1; // Force redraw on show
               }
               if (changed) InvalidateRect(g_hLockWindow, NULL, FALSE);
               SetWindowPos(g_hLockWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
          }
     } else {
          if (g_hLockWindow && IsWindowVisible(g_hLockWindow)) {
               ShowWindow(g_hLockWindow, SW_HIDE);
               ShowCursor(TRUE);
          }
     }
}

void set_timer_window(int enable, const char* time_str) {
     EnterCriticalSection(&g_cs);
     // Auto-show if time is low (< 1 minute)
     int h, m, s;
     if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
          if (h == 0 && m == 0 && s < 60 && timer_hidden) {
               timer_hidden = 0;
               if (g_hTimerWindow) ShowWindow(g_hTimerWindow, SW_SHOWNOACTIVATE);
          }
     }
     strncpy(g_TimerText, time_str, sizeof(g_TimerText) - 1);
     LeaveCriticalSection(&g_cs);

     if (enable) {
          if (g_hTimerWindow) {
               EnterCriticalSection(&g_cs);
               int hidden = timer_hidden;
               LeaveCriticalSection(&g_cs);
               
               if (!hidden && !IsWindowVisible(g_hTimerWindow)) {
                    ShowWindow(g_hTimerWindow, SW_SHOWNOACTIVATE);
               }
               InvalidateRect(g_hTimerWindow, NULL, FALSE);
          }
     } else {
          if (g_hTimerWindow) {
               if (IsWindowVisible(g_hTimerWindow)) ShowWindow(g_hTimerWindow, SW_HIDE);
               EnterCriticalSection(&g_cs);
               timer_hidden = 0;
               LeaveCriticalSection(&g_cs);
          }
     }
}

void update_window_events() {
     // Message loop is now handled in GuiThreadProc
}

void toggle_console() {
     HWND hConsole = GetConsoleWindow();
     if (IsWindowVisible(hConsole)) {
          ShowWindow(hConsole, SW_HIDE);
     } else {
          ShowWindow(hConsole, SW_SHOW);
          SetForegroundWindow(hConsole);
     }
}