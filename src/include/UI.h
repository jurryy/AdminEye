#ifndef TUI_H
#define TUI_H

#include "PC.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>

#include "Time.h"
#include "Util.h"
#include <string.h>
#include <stdio.h>

// Macro Polo
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#define TIMEOUT 12000
typedef enum
{
     COL_RESET,
     COL_RED,
     COL_GREEN,
     COL_YELLOW,
     COL_BLUE,
     COL_GREY,
     COL_ORANGE
} TextColor;

typedef struct
{
     int16_t width;
     int16_t height;
} ConsoleSize;

typedef struct
{
     int total_pages;
     int items_per_row;
     int items_per_page;
     int rows_per_page;
} UIGridLayout;

ConsoleSize getConsoleSize();
void clear_screen_cmd();
void clear_screen();
void move_cursor(int row, int col);
void hide_cursor();
void show_cursor();
const char* color(TextColor colorName);

void clean_row(int row, int width);
UIGridLayout draw_ui(PC* pcList, int total_count, int max_cap, ConsoleSize size, int8_t current_page, int8_t index, int8_t viewing_conn_info, char* promt);
void refresh_ui(ConsoleSize size);
void disable_echo();
void enable_echo();
void draw_scanner(DiscoveredPC* found, int count, int selected_idx, ConsoleSize size);

#endif