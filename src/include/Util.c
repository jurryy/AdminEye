#include "Util.h" // Ensure this matches your header file name
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// --- CROSS-PLATFORM INCLUDES ---
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

struct termios orig_termios;
#endif

void disable_raw_mode() {
#ifndef _WIN32
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
	printf("\033[?25h");
}

void console_init() {
	setbuf(stdout, NULL);
	printf("\033[?25l");

#ifndef _WIN32
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disable_raw_mode);

	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_iflag &= ~(ICRNL | IXON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

void console_cleanup() {
	disable_raw_mode();
}

int kbhit_fast() {
#ifdef _WIN32
	return _kbhit();
#else
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
#endif
}

int read_key() {
	int c = 0;

#ifdef _WIN32
	if (!_kbhit())
		return KEY_NONE;
	c = _getch();
	if (c == 0 || c == 224) {
		int arrow = _getch();
		switch (arrow) {
			case 72:
				return KEY_UP;
			case 80:
				return KEY_DOWN;
			case 77:
				return KEY_RIGHT;
			case 75:
				return KEY_LEFT;
		}
		return KEY_NONE;
	}
	if (c == 8)
		return KEY_BACKSPACE;

#else
	if (read(STDIN_FILENO, &c, 1) <= 0)
		return KEY_NONE;

	if (c == '\033') {
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return KEY_ESC;
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return KEY_ESC;

		if (seq[0] == '[') {
			switch (seq[1]) {
				case 'A':
					return KEY_UP;
				case 'B':
					return KEY_DOWN;
				case 'C':
					return KEY_RIGHT;
				case 'D':
					return KEY_LEFT;
			}
		}
		return KEY_ESC;
	}

	if (c == 127 || c == 8)
		return KEY_BACKSPACE;
#endif

	if (c == 13 || c == 10)
		return KEY_ENTER;
	if (c == 27)
		return KEY_ESC;
	return c;
}

void handle_text_input(int key, char* buffer, int max_len) {
	int len = strlen(buffer);
	if ((key == 8 || key == 127 || key == '\b' || key == KEY_BACKSPACE) && len > 0) {
		buffer[len - 1] = '\0';
	}
	else if (key >= 32 && key <= 126 && len < max_len) {
		buffer[len] = (char)key;
		buffer[len + 1] = '\0';
	}
}

void handle_ip_input(int key, char* buffer) {
	int len = strlen(buffer);

	if ((key == 8 || key == 127 || key == '\b' || key == KEY_BACKSPACE)) {
		if (len > 0) buffer[len - 1] = '\0';
		return;
	}

	// Filter non-printable
	if (key < 32 || key > 126) return;

	if (!isdigit(key) && key != '.') return;

	if (len >= 15) return;

	if (key == '.') {
		// 1. Check max dots (3)
		int dots = 0;
		for (int i = 0; i < len; i++) {
			if (buffer[i] == '.') dots++;
		}
		if (dots >= 3) return;

		// 2. Prevent starting with dot
		if (len == 0) return;

		// 3. Prevent consecutive dots
		if (buffer[len - 1] == '.') return;

		// Append
		buffer[len] = '.';
		buffer[len + 1] = '\0';
	}
	else if (isdigit(key)) {
		// Find current octet start
		int octet_start = 0;
		for (int i = len - 1; i >= 0; i--) {
			if (buffer[i] == '.') {
				octet_start = i + 1;
				break;
			}
		}

		// Check octet value (must be <= 255)
		char temp[4] = { 0 };
		int j = 0;
		for (int i = octet_start; i < len; i++) {
			if (j < 3) temp[j++] = buffer[i];
		}
		if (j >= 3) return; // Max 3 digits per octet

		temp[j] = (char)key;
		temp[j + 1] = '\0';

		if (atoi(temp) > 255) return;

		// Append
		buffer[len] = (char)key;
		buffer[len + 1] = '\0';
	}
}

void get_dashboard_time(int64_t timestamp, char* buffer, FormatTime type) {
	if (timestamp < 1) {
		sprintf(buffer, "--:--:--");
		return;
	}
	if (type == FORMAT_DURATION) {
		long long total_seconds = timestamp / 1000;
		long long hours = total_seconds / 3600;
		long long minutes = (total_seconds % 3600) / 60;
		long long seconds = total_seconds % 60;
		sprintf(buffer, "%02lld:%02lld:%02lld", hours, minutes, seconds);
	}
	else {
		time_t raw_time = (time_t)(timestamp / 1000);
		struct tm* info = localtime(&raw_time);
		if (type == FORMAT_12HF)
			strftime(buffer, 20, "%I:%M:%S %p", info);
		else
			strftime(buffer, 20, "%H:%M:%S", info);
	}
}

void get_unique_name(char* dest, size_t size, const char* input, PC* list, int count, int skip_idx) {
	snprintf(dest, size, "%s", input);
	int suffix = 2;
	while (1) {
		int exists = 0;
		for (int i = 0; i < count; i++) {
			if (i == skip_idx) continue;
			if (strcmp(list[i].name, dest) == 0) {
				exists = 1;
				break;
			}
		}
		if (!exists) break;
		snprintf(dest, size, "%s (%d)", input, suffix++);
	}
}