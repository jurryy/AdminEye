#ifndef TIME_H
#define TIME_H
#include <stdint.h>
#include <stdio.h>
void sleep_ms(int16_t ms);
char* format_duration(int64_t ms);
#endif