#ifndef UTILS_H
#define UTILS_H

#include <stdint.h> // For int64_t
#include <stddef.h> // For size_t
#include "PC.h"

#define KEY_NONE 0

#define KEY_ENTER 10
#define KEY_ESC 27

#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_LEFT 1003
#define KEY_RIGHT 1004
#define KEY_BACKSPACE 1005

typedef enum
{
     FORMAT_DURATION,
     FORMAT_12HF,
     FORMAT_24HF
} FormatTime;

void console_init(void);
void console_cleanup(void);

int read_key(void);

void get_dashboard_time(int64_t timestamp, char* buffer, FormatTime type);
void handle_text_input(int key, char* buffer, int max_len);
void handle_ip_input(int key, char* buffer);
void get_unique_name(char* dest, size_t size, const char* input, PC* list, int count, int skip_idx);

#endif