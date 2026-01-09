#ifndef WINDOW_H
#define WINDOW_H

#include "../UI.h"
#include "../Time.h"

// Colors
#define C_RESET  COL_RESET
#define C_GREEN  COL_GREEN
#define C_RED    COL_RED
#define C_YELLOW COL_YELLOW
#define C_BLUE   COL_BLUE
#define C_WHITE  COL_RESET

void init_window_system();
void set_window_lockdown(int enable, const char* title, const char* subtitle);
void set_timer_window(int enable, const char* time_str);
void update_window_events();
void get_console_size(int* width, int* height);
long long get_current_time_ms();
void console_gotoxy(int x, int y);
void console_set_color(int color_code);
void console_clear();
void toggle_console();
void toggle_timer();

#include <sys/ioctl.h>
#include <unistd.h>
#endif