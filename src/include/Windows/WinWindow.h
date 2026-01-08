#ifndef WINWINDOW_H
#define WINWINDOW_H

// Colors
#define C_RESET  0
#define C_GREEN  1
#define C_RED    2
#define C_YELLOW 3
#define C_BLUE   4
#define C_WHITE  5

void init_window_system();
void set_window_lockdown(int enable, const char* title, const char* subtitle);
void set_timer_window(int enable, const char* time_str);
void update_window_events();
void get_console_size(int* width, int* height);
long long get_current_time_ms();
void console_gotoxy(int x, int y);
void console_set_color(int color_code);
void console_clear();
void sleep_ms(int ms);
void toggle_console();

#ifdef _WIN32
#include <windows.h>
#endif

#endif