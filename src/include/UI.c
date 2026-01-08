#include "UI.h"
#include "PC.h"
#define CLEAR_CMD system("clear")

void clear_screen_cmd() {
	CLEAR_CMD;
}

void clear_screen() {
	printf("\033[2J\033[H");
	fflush(stdout);
}

void move_cursor(int row, int col) {
	printf("\033[%d;%dH", row + 1, col + 1);
}

void hide_cursor() {
	printf("\033[?25l");
}

void show_cursor() {
	printf("\033[?25h");
}

void clean_row(int row, int width) {
	move_cursor(row, 0);
	for (int i = 0; i < width; i++)
		printf(" ");
	move_cursor(row, 0);
}

ConsoleSize getConsoleSize() {
	ConsoleSize size = { 0, 0 };
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	size.width = (int16_t)w.ws_col;
	size.height = (int16_t)w.ws_row;
	return size;
}

const char* color(TextColor colorName) {
	switch (colorName) {
		case COL_RESET:
			return "\033[0m";
		case COL_RED:
			return "\033[1;31m";
		case COL_GREEN:
			return "\033[1;32m";
		case COL_YELLOW:
			return "\033[1;33m";
		case COL_BLUE:
			return "\033[1;34m";
		case COL_GREY:
			return "\033[0;90m";
		case COL_ORANGE:
			return "\033[38;5;208m";
		default:
			return "";
	}
}

void disable_echo() {
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void enable_echo() {
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag |= ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void refresh_bound(ConsoleSize size) {
	char timeStr[20];
	get_dashboard_time(getTimestamp(), timeStr, FORMAT_12HF);
	int center_col = (size.width - strlen(timeStr)) / 2;
	if (center_col < 0)
		center_col = 0;
	move_cursor(0, center_col);
	printf("%s%s%s", color(COL_YELLOW), timeStr, color(COL_RESET));

	move_cursor(1, 0);
	for (int k = 0; k < size.width; k++)
		printf("-");

	move_cursor(size.height - 5, 0);
	for (int k = 0; k < size.width; k++)
		printf("-");
}

void refresh_ui(ConsoleSize size) {
	disable_echo();
	refresh_bound(size);
}

int8_t _lastIndex = 0, _lastPromptLen = 0, _viewing_conn_info = 0;
UIGridLayout draw_ui(PC* pcList, int total_count, int max_cap, ConsoleSize size, int8_t current_page, int8_t selected_idx, int8_t view_conn_info, char* prompt) {
	UIGridLayout layout = { 0 };
	if (total_count == 0) {
		layout.total_pages = 1;
		// return layout;
	}

	int card_width = 45;
	int gap_x = 4;
	int row_height = 8;
	int total_item_width = card_width + gap_x;

	int grid_height = size.height - 4;
	int items_per_row = (size.width - 2) / total_item_width;
	if (items_per_row < 1)
		items_per_row = 1;

	int max_rows = (grid_height - 2) / row_height;
	if (max_rows < 1)
		max_rows = 1;
	int items_per_page = items_per_row * max_rows;

	layout.items_per_row = items_per_row;
	layout.rows_per_page = max_rows;
	layout.items_per_page = items_per_page;
	layout.total_pages = (total_count + items_per_page - 1) / items_per_page;
	if (layout.total_pages < 1)
		layout.total_pages = 1;

	// Centering math tsk hate math
	int total_block_width = (items_per_row * total_item_width) - gap_x;
	int start_x = (size.width - total_block_width) / 2;
	if (start_x < 2)
		start_x = 2;
	int start_y = 2;

	int start_index = current_page * items_per_page;
	int end_index = start_index + items_per_page;
	if (end_index > total_count)
		end_index = total_count;

	for (int i = start_index; i < end_index; i++) {
		PC pc = pcList[i];
		int is_selected = (i == selected_idx);
		int relative_i = i - start_index;
		int r = relative_i / items_per_row;
		int c = relative_i % items_per_row;
		int x = start_x + (c * total_item_width);
		int y = start_y + (r * row_height);

		// Cursor wow!!
		move_cursor(y, x - 2);
		if (is_selected)
			printf("\033[1;36m>\033[1;37m");
		else
			printf(" ");

		// Status
		move_cursor(y, x + 2);
		printf("%s%s%s",
			  color(pc.status == STATUS_CONNECTED
				   ? COL_GREEN
				   : pc.status == STATUS_BLOCKED
				   ? COL_BLUE
				   : COL_RED),
			  pc.status == STATUS_CONNECTED
			  ? "[C]"
			  : pc.status == STATUS_BLOCKED
			  ? "[B]"
			  : "[D]",
			  color(COL_RESET)
		);

		char* displayName = (strlen(pc.name) > 0) ? pc.name : "[EMPTY SLOT]";
		int name_pad = (card_width - strlen(displayName)) / 2;
		move_cursor(y, x + name_pad);
		printf("%s%s%s", is_selected ? "\x1b[1m" : "", displayName, color(COL_RESET));

		char* status_str = "";
		const char* status_col = "";
		switch (pc.state) {
			case STATE_BROKEN:
				status_str = "[BROKEN]";
				status_col = color(COL_RED);
				break;
			case STATE_VACANT:
				status_str = "[VACANT]";
				status_col = color(COL_BLUE);
				break;
			case STATE_OCCUPIED:
				status_str = "[OCCUPIED]";
				status_col = color(COL_GREEN);
				break;
		}
		move_cursor(y, x + card_width - strlen(status_str));
		printf("%s%s\033[0m", status_col, status_str);

		move_cursor(y + 1, x);
		printf("\033[0;90m");
		for (int k = 0; k < card_width; k++)
			printf("%s%s-", is_selected ? color(COL_RESET) : "", is_selected ? "\x1b[1m" : "\033[0;90m");
		printf("%s", color(COL_RESET));

		int show_details = (is_selected && view_conn_info);
		if (show_details) {
			move_cursor(y + 2, x);
			int ipLen = card_width - strlen(pc.ip) - printf("IP ") - 1;
			printf("\033[0;90m");
			for (int d = 0; d < ipLen; d++) printf("-");
			printf("%s %s", color(COL_RESET), pc.ip);
			move_cursor(y + 3, x);
			char timeoutBuf[32];
			int64_t elapsed = (pc.last_seen > 0) ? (getTimestamp() - pc.last_seen) : 0;
			get_dashboard_time(elapsed, timeoutBuf, FORMAT_DURATION);
			int timeLen = card_width - strlen(timeoutBuf) - printf("Last Rx ") - 1;
			printf("\033[0;90m");
			for (int d = 0; d < timeLen; d++) printf("-");
			printf("%s %s", (elapsed > TIMEOUT) ? color(COL_RED) : color(COL_RESET), timeoutBuf);
			for (int r = 4; r <= 6; r++) {
				move_cursor(y + r, x);
				printf("%-*s", card_width, " ");
			}
		}
		else {
			int row_idx = 2;
			if (pc.state == STATE_OCCUPIED) {
				move_cursor(y + row_idx++, x);
				printf("User:    %-*s", card_width - 7, pc.user);
				if (strlen(pc.note) > 0) {
					move_cursor(y + row_idx++, x);
					printf("Note:    %-*s", card_width - 7, pc.note);
				}

				char startBuf[32], endBuf[32];
				get_dashboard_time(pc.startedAt, startBuf, FORMAT_12HF);
				get_dashboard_time(pc.untilTimestamp, endBuf, FORMAT_12HF);

				move_cursor(y + row_idx++, x);
				int dash_len = card_width - strlen(endBuf) - printf("Start:   %s ", startBuf) - 1;
				printf("\033[0;90m");
				for (int d = 0; d < dash_len; d++) printf("-");
				printf("%s %s", color(COL_RESET), endBuf);
				int64_t rem = pc.untilTimestamp - getTimestamp();
				move_cursor(y + row_idx++, x);

				char timeLabel[64];
				int visibleLen = 0;
				if (rem <= 0) {
					if (pc.untilTimestamp == -1) sprintf(timeLabel, "Time:    --:--:--    ");
					else sprintf(timeLabel, "Time:    EXPIRED     ");
					visibleLen = strlen(timeLabel);
					printf("%s%s%s", color(COL_RED), timeLabel, color(COL_RESET));
				}
				else {
					char durBuf[32];
					get_dashboard_time(rem, durBuf, FORMAT_DURATION);
					sprintf(timeLabel, "Time:    %s", durBuf);
					visibleLen = strlen(timeLabel);
					printf("%sTime:    %s%s", color(COL_GREEN), durBuf, color(COL_RESET));
				}

				for (int s = 0; s < card_width - visibleLen; s++) printf(" ");
				if (pc.runtime > 0 && pc.untilTimestamp < getTimestamp()) {
					char runtimeBuf[32];
					get_dashboard_time(pc.runtime, runtimeBuf, FORMAT_DURATION);
					move_cursor(y + row_idx++, x);
					printf("%sRuntime: %s", color(COL_GREEN), runtimeBuf);
					int rtLen = 9 + strlen(runtimeBuf);
					printf("%s", color(COL_RESET));
					for (int s = 0; s < card_width - rtLen; s++) printf(" ");
				}
			}
			else if (pc.state == STATE_VACANT) {
				if (strlen(pc.note) > 0) {
					move_cursor(y + row_idx++, x);
					int dash_len = card_width - strlen(pc.note) - printf("Note: ") - 1;
					if (dash_len < 0) dash_len = 0;
					printf("\033[0;90m");
					for (int d = 0; d < dash_len; d++) printf("-");
					printf("%s %s", color(COL_RESET), pc.note);
				}
			}
			else if (pc.state == STATE_BROKEN) {
				move_cursor(y + row_idx++, x);
				char* noteText = strlen(pc.note) > 0 ? pc.note : "None";
				printf("\033[0;31mReason:    %s\033[0m", noteText);
				int visLen = 11 + strlen(noteText);
				for (int s = 0; s < card_width - visLen; s++) printf(" ");
			}

			int max_card_rows = 6;
			for (; row_idx <= max_card_rows; row_idx++) {
				move_cursor(y + row_idx, x);
				printf("%-*s", card_width, " ");
			}
		}

		if (c < items_per_row - 1) {
			printf("\033[0;90m");
			int splitter_x = x + card_width + (gap_x / 2);
			for (int h = 0; h < row_height - 1; h++) {
				move_cursor(y + h, splitter_x);
				printf("|");
			}
			printf("%s", color(COL_RESET));
		}
	}

	refresh_bound(size);

	char slotBuf[32];
	sprintf(slotBuf, "PC Slot: %d/%d", total_count, max_cap);
	move_cursor(0, 2);
	printf("%s%s%s", color(COL_BLUE), slotBuf, color(COL_RESET));

	int activePcs = 0, brokenPCs = 0;
	for (int i = 0; i < total_count; i++) {
		if (pcList[i].state == STATE_OCCUPIED && pcList[i].untilTimestamp > 0)
			activePcs++;
		else if (pcList[i].state == STATE_BROKEN)
			brokenPCs++;
	}
	move_cursor(0, strlen(slotBuf) + 5);
	printf("%sA: %d  %sB: %d", color(COL_GREEN), activePcs, color(COL_RED), brokenPCs);

	char page_footer[] = "[Z/X] Page %d/%d";
	move_cursor(0, (size.width - strlen(page_footer)));
	printf(page_footer, current_page + 1, layout.total_pages);
	printf("%s", color(COL_RESET));

	int valid_idx = (selected_idx >= 0 && selected_idx < max_cap);
	int has_pc = (valid_idx && strlen(pcList[selected_idx].name) > 0);

	move_cursor(size.height - 4, 2);
	if (has_pc) {
		printf("[ESC] Quit  |  %s[I] Conn Info  |  %s[R] Rename PC",
			  view_conn_info ? color(COL_GREEN) : color(COL_ORANGE), color(COL_BLUE));
		if (pcList[selected_idx].state != STATE_OCCUPIED)
			printf("  %s[A] Edit IP", color(COL_ORANGE));
		printf("  %s[D] Delete  %s[P] Add PC at slot [%d]", color(COL_RED), color(COL_GREEN), selected_idx + 1);
	}
	else
		printf("[ESC] Quit  |  %s[P] Add PC at slot [%d]",
			  color(COL_GREEN), selected_idx + 1);
	printf("\033[K"); // Clear the rest of the line to prevent artifacts

	if (selected_idx != _lastIndex)
		clean_row(size.height - 3, size.width);

	move_cursor(size.height - 3, 2);
	if (has_pc) {
		PC sel = pcList[selected_idx];
		if (sel.state == STATE_VACANT)
			printf("\033[1;32m[Enter] Add User\033[0m  |  \033[1;33m[B] Mark Broken");
		else if (sel.state == STATE_BROKEN)
			printf("\033[1;33m[Enter] Fix PC\033[0m  |  [E] Edit Reason");
		else
			printf("\033[1;32m[Enter] Edit Time  |  \033[1;31m[S] Stop Session\033[0m  |  [N] Edit Note");
	}

	if (selected_idx != _lastIndex)
		clean_row(size.height - 2, size.width);
	if (strlen(prompt) < 1 || _lastPromptLen != strlen(prompt))
		clean_row(size.height - 2, size.width);

	if (prompt != NULL && strlen(prompt) > 0) {
		move_cursor(size.height - 2, 2);
		printf("%s", prompt);
	}

	move_cursor(size.height - 1, 2);
	printf("%s>> ", color(COL_RESET));
	_lastIndex = selected_idx;
	_lastPromptLen = strlen(prompt);
	_viewing_conn_info = view_conn_info;
	return layout;
}

// ... PC Scanner ...
void draw_scanner(DiscoveredPC* found, int count, int selected_idx, ConsoleSize size) {
	int box_w = 54;
	int box_h = (count > 0 ? count : 1) + 4; // Dynamic height

	// Center the box
	int sx = (size.width - box_w) / 2;
	int sy = (size.height - box_h) / 2;
	if (sx < 0) sx = 0;
	if (sy < 0) sy = 0;

	// 1. Draw Shadow (for depth)
	printf("\033[0;90m");
	for (int i = 1; i <= box_h; i++) {
		move_cursor(sy + i, sx + 2);
		for (int j = 0; j < box_w; j++) printf("▒");
	}

	// 2. Draw Box Background & Border
	printf("\033[0m");
	for (int i = 0; i < box_h; i++) {
		move_cursor(sy + i, sx);
		if (i == 0) {
			printf("\033[1;37m┌");
			for (int k = 0; k < box_w - 2; k++) printf("─");
			printf("┐");
		}
		else if (i == box_h - 1) {
			printf("└");
			for (int k = 0; k < box_w - 2; k++) printf("─");
			printf("┘\033[0m");
		}
		else {
			// Clear the inside of the box with spaces
			printf("│%*s│", box_w - 2, " ");
		}
	}

	// 3. Draw Title
	move_cursor(sy, sx + 2);
	printf("\033[1;33m SCAN RESULTS (%d) \033[0m", count);

	// 4. Draw List
	if (count == 0) {
		move_cursor(sy + 2, sx + 2);
		printf("\033[1;31mNo devices found.\033[0m");
	}
	else {
		for (int i = 0; i < count; i++) {
			move_cursor(sy + 2 + i, sx + 2);
			if (i == selected_idx) {
				printf("\033[1;32m> %-20s  %-15s\033[0m", found[i].name, found[i].ip);
			}
			else {
				printf("  %-20s  \033[0;90m%-15s\033[0m", found[i].name, found[i].ip);
			}
		}
	}
}