// Compile with: gcc WinClient.c include/WinWin.c -o client.exe -lws2_32 -lgdi32 -ladvapi32 -luser32

#define _WIN32_WINNT 0x0500
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/PC.h"
#include "include/Windows/WinWindow.h"

// Explicit declaration to fix implicit declaration warning/linker error
SHORT WINAPI MyGetGetAsyncKeyState(int vKey) {
    typedef SHORT (WINAPI *PGETASYNCKEYSTATE)(int);
    static PGETASYNCKEYSTATE pGetGetAsyncKeyState = NULL;
    if (!pGetGetAsyncKeyState) {
        HMODULE hUser32 = GetModuleHandle("user32.dll");
        if (hUser32) pGetGetAsyncKeyState = (PGETASYNCKEYSTATE)GetProcAddress(hUser32, "GetGetAsyncKeyState");
    }
    return pGetGetAsyncKeyState ? pGetGetAsyncKeyState(vKey) : 0;
}
#define GetGetAsyncKeyState MyGetGetAsyncKeyState

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

#define DEFAULT_PORT 8888
#define MAX_BUFFER 1024

// --- STATE MANAGEMENT ---
typedef struct {
    char pc_name[32];
    char server_pc_name[64];
    char server_ip[32];
    int port;
    int connected;
    long start_time;
    long long remaining_time;
    long long session_end_at;
    long long last_rx;
    PCState remote_state;
    char user[32];
    char note[64];
} AppState;

AppState app;

void get_time_str(char* buffer) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    sprintf(buffer, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}

void scan_network(SOCKET sock) {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(app.port);
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    char packet[64];
    snprintf(packet, sizeof(packet), "WHOISSERVER|%s", app.pc_name);
    sendto(sock, packet, strlen(packet), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
}

HHOOK hKeyboardHook = NULL;
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        if ((p->vkCode == VK_TAB && (p->flags & LLKHF_ALTDOWN)) || // Alt+Tab
            (p->vkCode == VK_ESCAPE && (p->flags & LLKHF_ALTDOWN)) || // Alt+Esc
            (p->vkCode == VK_ESCAPE && (GetGetAsyncKeyState(VK_CONTROL) & 0x8000)) || // Ctrl+Esc
            (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) || // Windows Keys
            (p->vkCode == VK_F4 && (p->flags & LLKHF_ALTDOWN))) { // Alt+F4
            return 1;
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

void set_disable_lock_workstation(int disable) {
    HKEY hKey;
    const char* keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
    
    if (RegCreateKeyEx(HKEY_CURRENT_USER, keyPath, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (disable) {
            DWORD value = 1;
            RegSetValueEx(hKey, "DisableLockWorkstation", 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
        } else {
            RegDeleteValue(hKey, "DisableLockWorkstation");
        }
        RegCloseKey(hKey);
    }
}

static int is_cursor_hidden = 0;
void draw_ui() {
    // Use console_gotoxy instead of ANSI \033[H
    console_gotoxy(0, 0);

    int width, height;
    get_console_size(&width, &height);
    if (width < 60) width = 60;

    // --- LOCKDOWN CHECK ---
    int locked = 0;
    char lock_msg[64] = "SYSTEM LOCKED";
    char sub_msg[128] = "";

    if (app.remote_state == STATE_OCCUPIED) {
        if (app.remaining_time > 0) {
            locked = 0;
        }
        else {
            locked = 1;
            strcpy(lock_msg, "TIME UP");
            strcpy(sub_msg, "Please contact admin.");
        }
    }
    else if (app.remote_state == STATE_BROKEN) {
        locked = 1;
        strcpy(lock_msg, "OUT OF ORDER");
        if (strlen(app.note) > 0) snprintf(sub_msg, sizeof(sub_msg), "Reason: %s", app.note);
    }
    else if (!app.connected) {
        locked = 1;
        strcpy(lock_msg, "CONNECTING...");
    }
    else if (app.remote_state == STATE_VACANT) {
        locked = 1;
        strcpy(lock_msg, "AVAILABLE");
        snprintf(sub_msg, sizeof(sub_msg), "PC Name: %s", app.pc_name);
    }

    set_window_lockdown(locked, lock_msg, sub_msg);
    static int is_os_lock_disabled = 0;
    if (locked) {
        RECT rect;
        GetWindowRect(GetDesktopWindow(), &rect);
        int cx = (rect.right - rect.left) / 2;
        int cy = (rect.bottom - rect.top) / 2;
        SetCursorPos(cx, cy);
        RECT clip = {cx, cy, cx + 1, cy + 1};
        ClipCursor(&clip);
        if (!is_cursor_hidden) {
            while (ShowCursor(FALSE) >= 0); // Force hide
            is_cursor_hidden = 1;
        }
        if (!hKeyboardHook) {
            hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
        }
        if (!is_os_lock_disabled) {
            set_disable_lock_workstation(1);
            is_os_lock_disabled = 1;
        }
        return;
    } else {
        ClipCursor(NULL);
        if (is_cursor_hidden) {
            while (ShowCursor(TRUE) < 0); // Force show
            is_cursor_hidden = 0;
        }
        if (hKeyboardHook) {
            UnhookWindowsHookEx(hKeyboardHook);
            hKeyboardHook = NULL;
        }
        if (is_os_lock_disabled) {
            set_disable_lock_workstation(0);
            is_os_lock_disabled = 0;
        }
    }

    // 2. Prepare Data
    char time_str[32];
    get_time_str(time_str);

    char rem_str[32] = "00:00:00";
    if (app.remaining_time > 0) {
        long long total_sec = app.remaining_time / 1000;
        int rh = (int)(total_sec / 3600);
        int rm = (int)((total_sec % 3600) / 60);
        int rs = (int)(total_sec % 60);
        sprintf(rem_str, "%02d:%02d:%02d", rh, rm, rs);
    }

    // --- TIMER WINDOW ---
    if (app.remote_state == STATE_OCCUPIED && app.remaining_time > 0) {
        set_timer_window(1, rem_str);
    }
    else {
        set_timer_window(0, "");
    }

    // Optimization: Limit console redraw rate to reduce CPU usage
    static long long last_draw = 0;
    long long now = get_current_time_ms();
    if (now - last_draw < 200) return; // 5 FPS cap for console
    last_draw = now;

    // 3. Draw Top Box
    console_set_color(C_WHITE);
    for (int i = 0; i < width; i++) putchar('-');
    putchar('\n');

    char buf[256];
    if (app.connected) {
        snprintf(buf, sizeof(buf), "CONNECTED TO SERVER");
        int vis = strlen(buf);
        int pad = (width - 2 - vis) / 2;
        printf("|%*s", pad, "");
        console_set_color(C_GREEN);
        printf("%s", buf);
        console_set_color(C_WHITE);
        printf("%*s|\n", width - 2 - vis - pad, "");
    }
    else {
        snprintf(buf, sizeof(buf), "Not connected to the server.");
        int vis = strlen(buf);
        int pad = (width - 2 - vis) / 2;
        printf("|%*s", pad, "");
        console_set_color(C_RED);
        printf("%s", buf);
        console_set_color(C_WHITE);
        printf("%*s|\n", width - 2 - vis - pad, "");
    }

    snprintf(buf, sizeof(buf), "Pinging to Port %d", app.port);
    int vis = strlen(buf);
    int pad = (width - 2 - vis) / 2;
    printf("|%*s%s%*s|\n", pad, "", buf, width - 2 - vis - pad, "");

    char ip_display[32];
    snprintf(ip_display, sizeof(ip_display), "%s", strlen(app.server_ip) > 0 ? app.server_ip : "---:---:---:---");
    snprintf(buf, sizeof(buf), "Server IP: %s", ip_display);
    vis = strlen(buf);
    pad = (width - 2 - vis) / 2;
    printf("|%*s%s%*s|\n", pad, "", buf, width - 2 - vis - pad, "");

    printf("|%*s|\n", width - 2, "");

    char name_val[128];
    if (app.connected && strlen(app.server_pc_name) > 0 && strcmp(app.server_pc_name, app.pc_name) != 0) {
        snprintf(name_val, sizeof(name_val), "%s [%s]", app.server_pc_name, app.pc_name);
    }
    else {
        strcpy(name_val, app.pc_name);
    }
    printf("|             ");
    console_set_color(C_YELLOW);
    printf("PC Name:    ");
    console_set_color(C_WHITE);
    printf("%-30s", name_val);
    printf("%*s|\n", width - 1 - 13 - 12 - 30, "");

    char status_label[16] = "Status";
    char status_value[32] = "Idle";
    int val_col = C_WHITE;

    if (app.connected || app.remote_state == STATE_OCCUPIED) {
        if (app.remote_state == STATE_OCCUPIED) {
            strcpy(status_label, "Remaining");
            strcpy(status_value, rem_str);
        }
        else if (app.remote_state == STATE_VACANT) {
            strcpy(status_value, "VACANT");
            val_col = C_GREEN;
        }
        else if (app.remote_state == STATE_BROKEN) {
            strcpy(status_value, "BROKEN");
            val_col = C_RED;
        }
    }

    printf("|             Time:       %-30s", time_str);
    printf("%*s|\n", width - 1 - 13 - 12 - 30, "");

    printf("|             %-10s: ", status_label);
    console_set_color(val_col);
    printf("%-30s", status_value);
    console_set_color(C_WHITE);
    printf("%*s|\n", width - 1 - 13 - 12 - 30, "");

    if (app.remote_state == STATE_OCCUPIED) {
        printf("|             User:       %-30s", app.user);
        printf("%*s|\n", width - 1 - 13 - 12 - 30, "");
        if (strlen(app.note) > 0) {
            printf("|             Note:       %-30s", app.note);
            printf("%*s|\n", width - 1 - 13 - 12 - 30, "");
        }
    }
    else if (app.remote_state == STATE_BROKEN) {
        if (strlen(app.note) > 0) {
            printf("|             Reason:     %-30s", app.note);
            printf("%*s|\n", width - 1 - 13 - 12 - 30, "");
        }
    }
    for (int i = 0; i < width; i++) putchar('-');
    putchar('\n');

    // Clear rest of line to avoid artifacts
    printf("                                                                                \r");
}

void disable_close_button() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        if (hMenu) {
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
            RemoveMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
        }
    }
}

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT ||
        dwCtrlType == CTRL_CLOSE_EVENT) {
        set_disable_lock_workstation(0); // Restore Win+L on exit
        return TRUE;
    }
    return FALSE;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // Init State
    strcpy(app.pc_name, "PC-Guest");
    memset(app.server_pc_name, 0, sizeof(app.server_pc_name));
    strcpy(app.server_ip, "");
    app.port = DEFAULT_PORT;
    app.connected = 0;
    app.start_time = 0;
    app.remaining_time = 0;
    app.session_end_at = 0;
    app.last_rx = 0;
    app.remote_state = STATE_VACANT;
    memset(app.user, 0, sizeof(app.user));
    memset(app.note, 0, sizeof(app.note));

    SOCKET sock;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return 1;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    struct sockaddr_in myAddr;
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(0);
    myAddr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&myAddr, sizeof(myAddr));

    // Hide Console and Disable Close Button
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    disable_close_button();
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    init_window_system();
    console_clear();

    long long last_ping = 0;
    int last_width = 0, last_height = 0, w, h;

    while (1) {
        long long now_ms = get_current_time_ms();

        if (app.connected && (now_ms - app.last_rx > 5000)) {
            app.connected = 0;
        }

        if (app.remote_state == STATE_OCCUPIED) {
            long long diff = app.session_end_at - now_ms;
            app.remaining_time = (diff > 0) ? diff : 0;
        }

        char netBuf[1024];
        struct sockaddr_in senderAddr;
        int addrLen = sizeof(senderAddr);

        if ((now_ms / 1000) - last_ping > 2) {
            if (app.connected) {
                struct sockaddr_in serverAddr = { 0 };
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(app.port);
                serverAddr.sin_addr.s_addr = inet_addr(app.server_ip);

                char packet[64];
                snprintf(packet, sizeof(packet), "HEARTBEAT|%s", app.pc_name);
                sendto(sock, packet, strlen(packet), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            }
            else {
                scan_network(sock);
            }
            last_ping = now_ms / 1000;
        }

        int n = recvfrom(sock, netBuf, 1024, 0, (struct sockaddr*)&senderAddr, &addrLen);
        if (n > 0) {
            netBuf[n] = '\0';
            if (strncmp(netBuf, "SYNC|", 5) == 0) {
                app.connected = 1;
                app.last_rx = now_ms;
                strcpy(app.server_ip, inet_ntoa(senderAddr.sin_addr));

                char* ptr = netBuf + 5;
                char* end = NULL;

                end = strchr(ptr, '|'); if (end) *end = '\0';
                strncpy(app.server_pc_name, ptr, 63); ptr = end ? end + 1 : NULL;

                if (ptr) { end = strchr(ptr, '|'); if (end) *end = '\0'; app.remaining_time = _atoi64(ptr); app.session_end_at = now_ms + app.remaining_time; ptr = end ? end + 1 : NULL; }
                if (ptr) { end = strchr(ptr, '|'); if (end) *end = '\0'; app.remote_state = (PCState)atoi(ptr); ptr = end ? end + 1 : NULL; }
                if (ptr) { end = strchr(ptr, '|'); if (end) *end = '\0'; strncpy(app.user, ptr, 31); ptr = end ? end + 1 : NULL; }
                if (ptr) { strncpy(app.note, ptr, 63); }
            }
            else if (strncmp(netBuf, "CMD|", 4) == 0) {
                if (strcmp(netBuf + 4, "SHUTDOWN") == 0) system("shutdown /s /t 0");
                else if (strcmp(netBuf + 4, "RESTART") == 0) system("shutdown /r /t 0");
                else if (strcmp(netBuf + 4, "SLEEP") == 0) system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
                else if (strcmp(netBuf + 4, "HIBERNATE") == 0) system("rundll32.exe powrprof.dll,SetSuspendState 1,1,0");
            }
        }

        get_console_size(&w, &h);
        if (w != last_width || h != last_height) {
            console_clear();
            last_width = w;
            last_height = h;
        }

        draw_ui();
        update_window_events();
        Sleep(50);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}