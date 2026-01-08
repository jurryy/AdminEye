#include "Time.h"
#ifdef _WIN32
#include <windows.h>
void sleep_ms(int16_t milliseconds)
{
	Sleep(milliseconds);
}

#else
#include <unistd.h>
void sleep_ms(int16_t milliseconds)
{
	usleep(milliseconds * 1000);
}

char* format_duration(int64_t ms)
{
	static char buffer[32];
	if (ms < 0)
		ms = 0;
	int total_seconds = (int)(ms / 1000);
	int hours = total_seconds / 3600;
	int minutes = (total_seconds % 3600) / 60;
	int seconds = total_seconds % 60;
	sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
	return buffer;
}
#endif