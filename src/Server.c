#include "include/PC.h"
#include "include/UI.h"
#include "include/Time.h"
#include "include/Util.h"
#include "include/Seam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DATA_FILE "AdminEye.dat"
#define KEY 0XFF
#define MAX_PC_SLOT 10

typedef enum
{
	MODE_BROWSING,
	MODE_EXIT_CONFIRM,
	MODE_ADD_USER,
	MODE_EDIT_TIME,
	MODE_STOP_SESSION,
	MODE_MARK_BROKEN,
	MODE_RESTORE_PC,
	MODE_EDIT_NOTE,
	MODE_EDIT_REASON,
	MODE_ADD_PC,
	MODE_ADD_PC_IP,
	MODE_RENAME_PC,
	MODE_EDIT_IP,
	MODE_DELETE_PC,

	MODE_CONNECTION_INFO,
	// PC Scanner
	MODE_SCAN_CHOICE,
	MODE_SCAN_RESULTS,
	MODE_SCAN_SELECT
} AppMode;

void update_prompt_message(char* msg, size_t size, AppMode mode, char* input, PC* sel, int pc_idx, int pc_count, int h, int m, int s, int step, char* tempName, char* errorMsg) {
	switch (mode) {
		case MODE_BROWSING:
			if (errorMsg && strlen(errorMsg) > 0)
				snprintf(msg, size, ">> %s%s%s", color(COL_RED), errorMsg, color(COL_RESET));
			else
				msg[0] = '\0'; // clear
			break;
		case MODE_EXIT_CONFIRM:
			snprintf(msg, size, "Do you want to exit?:\t\033[1;31m[ESC] Cancel  |  [ENTER] Exit");
			break;
		case MODE_STOP_SESSION:
			snprintf(msg, size, ">> Stop the session for %s?: \033[1;32m[Enter] Confirm  |  \033[1;31m[ESC] Cancel", sel->name);
			break;
		case MODE_MARK_BROKEN:
			snprintf(msg, size, ">> Mark this PC as broken/unavailable?: \033[1;32m[Enter] Confirm  |  \033[1;31m[ESC] Cancel");
			break;
		case MODE_RESTORE_PC:
			snprintf(msg, size, ">> Restore this PC as vacant?: \033[1;32m[Enter] Confirm  |  \033[1;31m[ESC] Cancel");
			break;
		case MODE_DELETE_PC:
			snprintf(msg, size, ">> %sDelete this PC Slot?: %s[Y] Accept%s  |  [ESC] Cancel", color(COL_RESET), color(COL_GREEN), color(COL_RESET));
			break;

			// Text Input Prompts
		case MODE_ADD_USER:
			snprintf(msg, size, ">> Enter User: %s_", input);
			break;
		case MODE_EDIT_NOTE:
			snprintf(msg, size, ">> Edit Note (50 chars): %s_", input);
			break;
		case MODE_EDIT_REASON:
			snprintf(msg, size, ">> Edit Reason (50 chars): %s_", input);
			break;
		case MODE_RENAME_PC:
			snprintf(msg, size, ">> New Name (10 chars): %s_", input);
			break;
		case MODE_ADD_PC:
			if (pc_count >= MAX_PC_SLOT)
				snprintf(msg, size, "%s! Max PC slots reached. Extend in settings.%s", color(COL_ORANGE), color(COL_RESET));
			else
				snprintf(msg, size, ">> Add PC Name at slot %d/%d? (10 chars): %s_", pc_idx + 1, pc_count, input);
			break;
		case MODE_ADD_PC_IP:
			if (errorMsg && strlen(errorMsg) > 0)
				snprintf(msg, size, ">> %s%s%sEnter IP Address for %s: %s_", color(COL_RED), errorMsg, color(COL_RESET), tempName, input);
			else
				snprintf(msg, size, ">> Enter IP Address for %s: %s_", tempName, input);
			break;
		case MODE_EDIT_IP:
			if (errorMsg && strlen(errorMsg) > 0)
				snprintf(msg, size, ">> %s%s%sEdit IP Address (15 chars): %s_", color(COL_RED), errorMsg, color(COL_RESET), input);
			else
				snprintf(msg, size, ">> Edit IP Address (15 chars): %s_", input);
			break;

			// Time Edit Prompt
		case MODE_EDIT_TIME: {
			char hC[20], mC[20], sC[20];
			strcpy(hC, (step == 0) ? "\033[1;30;47m" : "\033[0m");
			strcpy(mC, (step == 1) ? "\033[1;30;47m" : "\033[0m");
			strcpy(sC, (step == 2) ? "\033[1;30;47m" : "\033[0m");
			snprintf(msg, size, "Set Time Session:  %s %02d %s : %s %02d %s : %s %02d %s \033[0;90m[Arrows] Adjust   %s[ENTER] Confirm",
				    hC, h, "\033[0m", mC, m, "\033[0m", sC, s, "\033[0m", color(COL_GREEN));
		}
			break;

			// View Connection Info
		case MODE_CONNECTION_INFO:
			break;

			// PC Scanner
		case MODE_SCAN_CHOICE:
			snprintf(msg, size, ">> Add PC Method: \033[1;33m[M]\033[0m Manual Entry  |  \033[1;32m[S]\033[0m Auto-Scan Network  |  [ESC] Cancel");
			break;
		case MODE_SCAN_RESULTS:
			snprintf(msg, size, ">> Found Devices: \033[1;32m[A]\033[0m Add All  |  \033[1;33m[S]\033[0m Select One  |  [ESC] Cancel");
			break;
		case MODE_SCAN_SELECT:
			snprintf(msg, size, ">> Select PC: \033[0;90m[UP/DOWN]\033[0m Move  |  \033[1;32m[ENTER]\033[0m Add  |  [ESC] Back");
			break;
	}
}

// --- DATA PERSISTENCE ---
void xor_crypt(void* ptr, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] ^= KEY;
    }
}

void save_data(PC* pcList, int count) {
    FILE* fp = fopen(DATA_FILE, "wb");
    if (!fp) return;

    int c = count;
    xor_crypt(&c, sizeof(int));
    fwrite(&c, sizeof(int), 1, fp);

    PC* temp = malloc(sizeof(PC) * count);
    if (temp) {
        memcpy(temp, pcList, sizeof(PC) * count);
        xor_crypt(temp, sizeof(PC) * count);
        fwrite(temp, sizeof(PC), count, fp);
        free(temp);
    }
    fclose(fp);
}

int load_data(PC* pcList, int* count, int max_cap) {
    FILE* fp = fopen(DATA_FILE, "rb");
    if (!fp) return 0;

    int c;
    if (fread(&c, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    xor_crypt(&c, sizeof(int));

    if (c > max_cap) c = max_cap;
    *count = c;

    PC* temp = malloc(sizeof(PC) * c);
    if (temp) {
        if (fread(temp, sizeof(PC), c, fp) == c) {
            xor_crypt(temp, sizeof(PC) * c);
            memcpy(pcList, temp, sizeof(PC) * c);
        }
        free(temp);
    }
    fclose(fp);
    return 1;
}

void init_defaults(PC* pcList, int* count) {
    *count = 0; // Start empty or define default slots
    // If you want default slots:
    // *count = MAX_PC_SLOT; for(int i=0; i<*count; i++) { ... }
    save_data(pcList, *count);
}

int server_sock = -1;
int main() {
	console_init();
	int pcCount = MAX_PC_SLOT;
	PC pcList[pcCount];
    memset(pcList, 0, sizeof(pcList));

    if (!load_data(pcList, &pcCount, MAX_PC_SLOT)) {
        init_defaults(pcList, &pcCount);
    } else {
        // Reset transient status on load
        for(int i=0; i<pcCount; i++) {
            pcList[i].status = STATUS_DISCONNECTED;
            pcList[i].last_seen = 0;
        }
    }

	clear_screen();
	hide_cursor();
	disable_echo();
	ConsoleSize lastSize = getConsoleSize();
	refresh_ui(lastSize);

	int8_t current_page = 0, selected_index = 0;
	AppMode currentMode = MODE_BROWSING;
	int8_t viewing_conn_info = 0;
	char promptMsg[300] = { 0 }, input[250] = { 0 };
	char tempName[64] = { 0 };
	char errorMsg[64] = { 0 };

	int t_hrs = 1, t_min = 30, t_sec = 0, t_step = 0;
	UIGridLayout layout = { 0 };
	int kuto = 1;            // Only draw when true
	long long last_kuto = 0; // For clock updates

	DiscoveredPC scanResults[20];
	int scanCount = 0;
	int scan_select_idx = 0;
    int data_changed = 0;

	init_server_socket(&server_sock);

	while (1) {
		PC* sel = &pcList[selected_index];
		long long now = getTimestamp();
		if (now - last_kuto >= 1000) {
			kuto = 1;
			last_kuto = now;
		}

		// --- NETWORK LISTENER ---
		if (server_sock != -1) {
			char buf[1024];
			struct sockaddr_in sender;
			socklen_t slen = sizeof(sender);
			int n = recvfrom(server_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&sender, &slen);
			if (n > 0) {
				buf[n] = '\0';
				char sender_ip[32];
				inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));

				PC* foundPC = NULL;
				// 1. Update PC Status
				for (int i = 0; i < pcCount; i++) {
					if (strcmp(pcList[i].ip, sender_ip) == 0) {
						pcList[i].status = STATUS_CONNECTED;
						pcList[i].last_seen = getTimestamp();
						foundPC = &pcList[i];
					}
				}

				// 2. Reply to Discovery
				if (strncmp(buf, "WHOISSERVER", 11) == 0) {
					if (foundPC) {
						long long rem = 0;
						if (foundPC->state == STATE_OCCUPIED && foundPC->untilTimestamp > 0) {
							rem = foundPC->untilTimestamp - getTimestamp();
							if (rem < 0) rem = 0;
						}
						char sync_pkt[256];
						snprintf(sync_pkt, sizeof(sync_pkt), "SYNC|%s|%lld|%d|%s|%s", foundPC->name, rem, foundPC->state, foundPC->user, foundPC->note);
						sendto(server_sock, sync_pkt, strlen(sync_pkt), 0, (struct sockaddr*)&sender, slen);
					}
					else {
						char* reply = "ADMINEYE_SERVER_BEACON";
						sendto(server_sock, reply, strlen(reply), 0, (struct sockaddr*)&sender, slen);
					}
				}
				// 3. Reply to Heartbeat with SYNC data
				else if (strncmp(buf, "HEARTBEAT", 9) == 0 && foundPC) {
					long long rem = 0;
					if (foundPC->state == STATE_OCCUPIED && foundPC->untilTimestamp > 0) {
						rem = foundPC->untilTimestamp - getTimestamp();
						if (rem < 0) rem = 0;
					}
					char sync_pkt[256];
					snprintf(sync_pkt, sizeof(sync_pkt), "SYNC|%s|%lld|%d|%s|%s", foundPC->name, rem, foundPC->state, foundPC->user, foundPC->note);
					sendto(server_sock, sync_pkt, strlen(sync_pkt), 0, (struct sockaddr*)&sender, slen);
				}
			}
		}

		// --- TIMEOUT CHECK (5 Seconds) ---
		for (int i = 0; i < pcCount; i++) {
			if (pcList[i].status == STATUS_CONNECTED && (getTimestamp() - pcList[i].last_seen > TIMEOUT)) {
				pcList[i].status = STATUS_DISCONNECTED;
			}
		}

		ConsoleSize currentSize = getConsoleSize();
		if (currentSize.width != lastSize.width || currentSize.height != lastSize.height) {
			lastSize = currentSize;
			clear_screen();
			kuto = 1;
		}

		if (kuto) {
			update_prompt_message(promptMsg, sizeof(promptMsg), currentMode, input, sel, selected_index, pcCount, t_hrs, t_min, t_sec, t_step, tempName, errorMsg);
			layout = draw_ui(pcList, pcCount, MAX_PC_SLOT, lastSize, current_page, selected_index, viewing_conn_info, promptMsg);
			if (currentMode == MODE_SCAN_RESULTS || currentMode == MODE_SCAN_SELECT) {
				int hl = (currentMode == MODE_SCAN_SELECT) ? scan_select_idx : -1;
				draw_scanner(scanResults, scanCount, hl, lastSize);
			}
			kuto = 0;
		}

        if (data_changed) {
            save_data(pcList, pcCount);
            data_changed = 0;
        }

		int key;
		while ((key = read_key()) != KEY_NONE) {
			kuto = 1;
			if (key == KEY_ESC) {
				if (currentMode == MODE_BROWSING)
					currentMode = MODE_EXIT_CONFIRM;
				else {
					currentMode = MODE_BROWSING;
					memset(input, 0, sizeof(input));
					clear_screen();
				}
				continue;
			}

			switch (currentMode) {
				case MODE_BROWSING:
					if (strlen(errorMsg) > 0) memset(errorMsg, 0, sizeof(errorMsg));

					// Navigation
					if (key == KEY_RIGHT)
						selected_index++;
					else if (key == KEY_LEFT)
						selected_index--;
					else if (key == KEY_DOWN)
						selected_index += layout.items_per_row;
					else if (key == KEY_UP)
						selected_index -= layout.items_per_row;

					// Paging (Z/X)
					if (tolower(key) == 'z' && current_page > 0) {
						current_page--;
						selected_index = current_page * layout.items_per_page;
						clear_screen();
						refresh_ui(lastSize);
					}
					if (tolower(key) == 'x' && current_page < (layout.total_pages - 1)) {
						current_page++;
						selected_index = current_page * layout.items_per_page;
						clear_screen();
						refresh_ui(lastSize);
					}
					if (selected_index < 0)
						selected_index = 0;
					if (pcCount > 0 && selected_index >= pcCount)
						selected_index = pcCount - 1;
					else if (pcCount == 0)
						selected_index = 0;

					sel = &pcList[selected_index];
					if (layout.items_per_page > 0 && (selected_index / layout.items_per_page) != current_page) {
						current_page = selected_index / layout.items_per_page;
						clear_screen();
						refresh_ui(lastSize);
					}

					// Chorkats - Shortcuts kay pro tah
					int has_pc = (strlen(sel->name) > 0);
					if (key == KEY_ENTER) {
						if (sel->state == STATE_VACANT && has_pc) {
                            // Allow adding session even if disconnected (admin override)
                            // if (sel->status == STATUS_DISCONNECTED)
							// 	strcpy(errorMsg, "Cannot add session! PC is disconnected.");
							// else
							currentMode = MODE_ADD_USER;
						}
						else if (sel->state == STATE_BROKEN) {
							currentMode = MODE_RESTORE_PC;
                        }
						else if (sel->state == STATE_OCCUPIED) {
							if (sel->status == STATUS_DISCONNECTED)
								strcpy(errorMsg, "Cannot edit time! PC is disconnected.");
							else {
								currentMode = MODE_EDIT_TIME;
								long long rem = sel->untilTimestamp - getTimestamp();
								if (rem < 0)
									rem = 0;
								t_hrs = rem / 3600000LL;
								t_min = (rem % 3600000LL) / 60000LL;
								t_sec = (rem % 60000LL) / 1000LL;
							}
						}
					}
					// View Connection Info
					else if (tolower(key) == 'i' && has_pc)
						viewing_conn_info = viewing_conn_info == 1 ? 0 : 1;

					// Staph
					else if (tolower(key) == 's' && sel->state == STATE_OCCUPIED)
						currentMode = MODE_STOP_SESSION;
					// Edit Death Note
					else if (tolower(key) == 'n' && sel->state == STATE_OCCUPIED) {
						currentMode = MODE_EDIT_NOTE;
						strcpy(input, sel->note);
					}
					// Edit Reason - only if broken hearted :(
					else if (tolower(key) == 'e' && sel->state == STATE_BROKEN) {
						currentMode = MODE_EDIT_REASON;
						strcpy(input, sel->note);
					}
					// Mark kay broken
					else if (tolower(key) == 'b' && sel->state == STATE_VACANT && has_pc)
						currentMode = MODE_MARK_BROKEN;
					// Add PC
					else if (tolower(key) == 'p')
						currentMode = MODE_SCAN_CHOICE;
					// Rename PC Slot
					else if (tolower(key) == 'r' && has_pc) {
						currentMode = MODE_RENAME_PC;
						strcpy(input, sel->name);
					}
					// Edit IP
					else if (tolower(key) == 'a' && has_pc && sel->state != STATE_OCCUPIED) {
						currentMode = MODE_EDIT_IP;
						strcpy(input, sel->ip);
					}
					// Delete PC Slot
					else if (tolower(key) == 'd' && has_pc)
						currentMode = MODE_DELETE_PC;
					break;

				case MODE_CONNECTION_INFO:
					break;

				case MODE_EDIT_TIME:
					if (key == KEY_LEFT && t_step > 0)
						t_step--;
					else if (key == KEY_RIGHT && t_step < 2)
						t_step++;
					else if (key == KEY_UP || key == KEY_DOWN) {
						int dir = (key == KEY_UP) ? 1 : -1;
						if (t_step == 0) {
							t_hrs += dir;
							if (t_hrs < 0)
								t_hrs = 0;
							if (t_hrs > 24)
								t_hrs = 24;
						}
						else if (t_step == 1) {
							t_min += dir;
							if (t_min < 0)
								t_min = 59;
							if (t_min > 59)
								t_min = 0;
						}
						else {
							t_sec += (dir * 5);
							if (t_sec < 0)
								t_sec = 55;
							if (t_sec > 55)
								t_sec = 0;
						}
					}
					else if (key == KEY_ENTER) {
						if ((t_hrs + t_min + t_sec) > 0) {
							long long now = getTimestamp();
							long long dur = ((t_hrs * 3600) + (t_min * 60) + t_sec) * 1000LL;
							sel->runtime = now - sel->startedAt;
							sel->untilTimestamp = now + dur;
							if (sel->startedAt < 1)
								sel->startedAt = now;
							sel->state = STATE_OCCUPIED;
							t_hrs = 1;
							t_min = 30;
							t_sec = 0;
						}
                        data_changed = 1;
						currentMode = MODE_BROWSING;
					}
					break;

					// Confirmations
				case MODE_EXIT_CONFIRM:
					if (key == KEY_ENTER)
						goto exit_program;
					break;
				case MODE_STOP_SESSION:
				case MODE_RESTORE_PC:
					if (key == KEY_ENTER) {
						sel->state = STATE_VACANT;
						sel->user[0] = 0;
						sel->note[0] = 0;
						sel->startedAt = -1;
						sel->untilTimestamp = -1;
                        data_changed = 1;
						currentMode = MODE_BROWSING;
						clear_screen();
					}
					break;
				case MODE_MARK_BROKEN:
					if (key == KEY_ENTER) {
						sel->state = STATE_BROKEN;
                        data_changed = 1;
						currentMode = MODE_BROWSING;
					}
					break;
				case MODE_DELETE_PC:
					if (tolower(key) == 'y') {
						remove_pc(pcList, &pcCount, selected_index);
                        data_changed = 1;
						currentMode = MODE_BROWSING;
						selected_index = 0, current_page = 0;
						clear_screen();
					}
					break;

					// PC Scanner
				case MODE_SCAN_CHOICE:
					if (tolower(key) == 'm') {
						currentMode = MODE_ADD_PC; // Go to your manual entry
						memset(input, 0, sizeof(input));
					}
					else if (tolower(key) == 's') {
						// 1. Show "Scanning" immediately
						snprintf(promptMsg, sizeof(promptMsg), ">> \033[1;33mScanning network (5s)... Please wait...\033[0m");
						draw_ui(pcList, pcCount, MAX_PC_SLOT, lastSize, current_page, selected_index, viewing_conn_info, promptMsg);
						fflush(stdout); // Force text to screen

						// Close main socket temporarily to allow scanner to bind
						if (server_sock != -1) { close(server_sock); server_sock = -1; }

						// 2. Run blocking scan
						scanCount = scanClients(scanResults, 50, 5, pcList, pcCount);
						init_server_socket(&server_sock); // Re-open

						// 3. Switch to results
						currentMode = MODE_SCAN_RESULTS;
					}
					else if (key == KEY_ESC) currentMode = MODE_BROWSING;
					break;

				case MODE_SCAN_RESULTS:
					if (scanCount == 0) { // Nothing found
						if (key == KEY_ESC || key == KEY_ENTER) currentMode = MODE_BROWSING;
					}
					else {
						if (tolower(key) == 'a') {
							// Add All Logic
							for (int i = 0; i < scanCount; i++) {
								if (pcCount < MAX_PC_SLOT) {
									PC _tmp = { 0 };
									strcpy(_tmp.name, scanResults[i].name);
									strcpy(_tmp.ip, scanResults[i].ip);
									_tmp.state = STATE_VACANT;
									// Helper function to append
									insert_pc(pcList, &pcCount, MAX_PC_SLOT, pcCount, _tmp);
                                    data_changed = 1;
								}
							}
							currentMode = MODE_BROWSING;
						}
						else if (tolower(key) == 's') {
							currentMode = MODE_SCAN_SELECT;
							scan_select_idx = 0; // Reset popup cursor
						}
						else if (key == KEY_ESC) {
							currentMode = MODE_BROWSING;
							clear_screen(); // <--- ADD THIS
							refresh_ui(lastSize); // <--- AND THIS (Redraws borders/title)
						}
					}
					break;

				case MODE_SCAN_SELECT:
					if (key == KEY_UP && scan_select_idx > 0) scan_select_idx--;
					else if (key == KEY_DOWN && scan_select_idx < scanCount - 1) scan_select_idx++;
					else if (key == KEY_ESC) {
						currentMode = MODE_SCAN_RESULTS;
						clear_screen(); // <--- ADD THIS
						refresh_ui(lastSize); // <--- AND THIS
					}
					else if (key == KEY_ENTER) {
						if (pcCount < MAX_PC_SLOT) {
							PC _tmp = { 0 };
							strcpy(_tmp.name, scanResults[scan_select_idx].name);
							strcpy(_tmp.ip, scanResults[scan_select_idx].ip);
							_tmp.state = STATE_VACANT;

							// --- CHANGE IS HERE ---
							// We pass 'selected_index' as the 4th argument
							insert_pc(pcList, &pcCount, MAX_PC_SLOT, selected_index, _tmp);
                            data_changed = 1;

							// Cleanup UI
							currentMode = MODE_BROWSING;
							clear_screen();
							refresh_ui(lastSize);
						}
					}
					break;

					// --- TEXT INPUT MODES ---
				case MODE_ADD_USER:
				case MODE_EDIT_NOTE:
				case MODE_EDIT_REASON:
				case MODE_ADD_PC:
				case MODE_ADD_PC_IP:
				case MODE_RENAME_PC:
				case MODE_EDIT_IP:
					if (key == KEY_ENTER) {
						// 1. ADD USER
						if (currentMode == MODE_ADD_USER) {
							sel->state = STATE_OCCUPIED;
							sel->startedAt = -1;
							sel->untilTimestamp = -1;
							strcpy(sel->user, (strlen(input) > 0) ? input : "guest");
                            data_changed = 1;
						}
						// 2. EDIT NOTES / REASON
						else if (currentMode == MODE_EDIT_NOTE || currentMode == MODE_EDIT_REASON) {
							if (strlen(input) > 0) {
								strcpy(sel->note, input);
                                data_changed = 1;
                            }
						}
						// 3. RENAME PC
						else if (currentMode == MODE_RENAME_PC) {
							if (strlen(input) > 0) {
								get_unique_name(sel->name, sizeof(sel->name), input, pcList, pcCount, selected_index);
                                data_changed = 1;
                            }
						}
						// 4. ADD PC (Manual)
						else if (currentMode == MODE_ADD_PC) {
							if (strlen(input) > 0 && pcCount < MAX_PC_SLOT) {
								get_unique_name(tempName, sizeof(tempName), input, pcList, pcCount, -1);
								currentMode = MODE_ADD_PC_IP;
								memset(input, 0, sizeof(input));
								clear_screen();
								refresh_ui(lastSize);
								continue; // Skip cleanup to stay in input loop
							}
						}
						// 5. ADD PC IP (Manual Step 2)
						else if (currentMode == MODE_ADD_PC_IP) {
							struct sockaddr_in sa;
							if (inet_pton(AF_INET, input, &(sa.sin_addr)) == 1) {
								// Check duplicate IP
								int exists = 0;
								for (int i = 0; i < pcCount; i++) {
									if (strcmp(pcList[i].ip, input) == 0) {
										exists = 1;
										break;
									}
								}
								if (exists) {
									strcpy(errorMsg, "[IP Taken!] ");
									clear_screen();
									refresh_ui(lastSize);
									continue;
								}

								PC _tmp = { 0 };
								strcpy(_tmp.name, tempName);
								strcpy(_tmp.ip, input);
								_tmp.state = STATE_VACANT;
								// Insert at cursor position
								insert_pc(pcList, &pcCount, MAX_PC_SLOT, selected_index, _tmp);
                                data_changed = 1;
							}
							else {
								strcpy(errorMsg, "[Invalid IP!] ");
								clear_screen();
								refresh_ui(lastSize);
								continue; // Invalid IP, ignore Enter
							}
						}
						// 6. EDIT IP
						else if (currentMode == MODE_EDIT_IP) {
							struct sockaddr_in sa;
							if (inet_pton(AF_INET, input, &(sa.sin_addr)) == 1) {
								// Check duplicate IP
								int exists = 0;
								for (int i = 0; i < pcCount; i++) {
									if (i == selected_index) continue;
									if (strcmp(pcList[i].ip, input) == 0) {
										exists = 1;
										break;
									}
								}
								if (exists) {
									strcpy(errorMsg, "[IP Taken!] ");
									clear_screen();
									refresh_ui(lastSize);
									continue;
								}
								strcpy(sel->ip, input);
                                data_changed = 1;
							}
							else {
								strcpy(errorMsg, "[Invalid IP!] ");
								clear_screen();
								refresh_ui(lastSize);
								continue;
							}
						}

						// --- COMMON CLEANUP ---
						currentMode = MODE_BROWSING;
						memset(input, 0, sizeof(input));
						memset(errorMsg, 0, sizeof(errorMsg));

						// Force clear to remove any input prompts or artifacts
						clear_screen();
						refresh_ui(lastSize);
					}
					else {
						// Use generic helper for typing (Limits: 10 for PC/User, 30 for Notes)
						int limit = 10;
						if (currentMode == MODE_EDIT_NOTE || currentMode == MODE_EDIT_REASON) limit = 30;

						if (strlen(errorMsg) > 0) memset(errorMsg, 0, sizeof(errorMsg));

						if (currentMode == MODE_ADD_PC_IP || currentMode == MODE_EDIT_IP) {
							handle_ip_input(key, input);
						}
						else {
							handle_text_input(key, input, limit);
						}
					}
					break;
			}
		}
		usleep(10000);
	}

exit_program:
    save_data(pcList, pcCount);
	if (server_sock != -1) close(server_sock);
	console_cleanup();
	enable_echo();
	show_cursor();
	clear_screen();
	return 0;
}