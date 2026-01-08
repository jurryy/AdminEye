#define _POSIX_C_SOURCE 199309L
#include "PC.h"
#include <errno.h>

time_t getRemainingTime(PC pc) {
	if (strlen(pc.name) < 1 || (pc.untilTimestamp < 1 || pc.startedAt < 1))
		return -1;
	if (!getTimestamp() || pc.state == STATE_VACANT)
		return 0;
	return pc.untilTimestamp - getTimestamp();
}

int64_t getTimestamp() {
	struct timespec tms;
#ifdef _WIN32
	if (timespec_get(&tms, TIME_UTC) == 0)
		return -1;
#else
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif
	if (clock_gettime(CLOCK_MONOTONIC, &tms) != 0)
		return -1;
#endif
	return (int64_t)tms.tv_sec * 1000 + (int64_t)tms.tv_nsec / 1000000;
}

int insert_pc(PC* list, int* count, int max_capacity, int index, PC newItem) {
	if (*count >= max_capacity)
		return 0;
	if (index < 0)
		index = 0;
	if (index > *count)
		index = *count;
	for (int i = *count; i > index; i--)
		list[i] = list[i - 1];
	list[index] = newItem;
	(*count)++;
	return 1;
}

void remove_pc(PC* list, int* count, int index) {
	if (index < 0 || index >= *count)
		return;
	for (int i = index; i < *count - 1; i++)
		list[i] = list[i + 1];
	(*count)--;
	memset(&list[*count], 0, sizeof(PC));
}

void remove_all_pc(PC* list, int* count) {
	for (int i = 0; i < *count; ) {
		int is_null = (strlen(list[i].name) == 0);
		if (is_null)
			remove_pc(list, count, i);
		else
			i++;
	}
}

int scanClients(DiscoveredPC* results, int max_cap, int timeout_sec, PC* current_list, int current_count) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return 0;

	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8888);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(sock);
		return 0;
	}

	int count = 0;
	time_t start = time(NULL);

	while (time(NULL) - start < timeout_sec && count < max_cap) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		struct timeval tv = { 1, 0 }; // 1 second slice
		int ready = select(sock + 1, &fds, NULL, NULL, &tv);

		if (ready > 0) {
			char buf[128];
			struct sockaddr_in sender;
			socklen_t len = sizeof(sender);
			int n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&sender, &len);

			if (n > 0) {
				buf[n] = '\0';

				char* name_ptr = NULL;
				if (strncmp(buf, "WHOISSERVER|", 12) == 0) {
					name_ptr = buf + 12;
				}
				else if (strncmp(buf, "HEARTBEAT|", 10) == 0) {
					name_ptr = buf + 10;
				}

				if (name_ptr) {
					char ip_str[20];
					snprintf(ip_str, sizeof(ip_str), "%s", inet_ntoa(sender.sin_addr));

					// 1. Check if IP exists in CURRENT SCAN RESULTS
					int exists = 0;
					for (int i = 0; i < count; i++) {
						if (strcmp(results[i].ip, ip_str) == 0) {
							exists = 1;
							break;
						}
					}

					// 2. Check if IP exists in EXISTING PC LIST
					if (!exists && current_list) {
						for (int i = 0; i < current_count; i++) {
							if (strcmp(current_list[i].ip, ip_str) == 0) {
								exists = 1;
								break;
							}
						}
					}

					if (!exists) {
						// 3. Handle Name Conflicts
						char final_name[64];
						snprintf(final_name, sizeof(final_name), "%s", name_ptr);
						int suffix = 2;

						while (1) {
							int name_conflict = 0;
							if (current_list) {
								for (int i = 0; i < current_count; i++) {
									if (strcmp(current_list[i].name, final_name) == 0) name_conflict = 1;
								}
							}
							if (!name_conflict) {
								for (int i = 0; i < count; i++) {
									if (strcmp(results[i].name, final_name) == 0) name_conflict = 1;
								}
							}

							if (name_conflict) snprintf(final_name, sizeof(final_name), "%s (%d)", name_ptr, suffix++);
							else break;
						}

						snprintf(results[count].ip, sizeof(results[count].ip), "%s", ip_str);
						snprintf(results[count].name, sizeof(results[count].name), "%s", final_name);
						count++;
					}
				}
			}
		}
	}

	close(sock);
	return count;
}