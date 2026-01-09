#ifndef PC_H
#define PC_H

#include <stdio.h>   
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <sys/time.h>
#endif

typedef enum
{
     STATE_BROKEN = -1,
     STATE_VACANT = 0,
     STATE_OCCUPIED = 1,
} PCState;
typedef enum {
     STATUS_CONNECTED = 1,
     STATUS_DISCONNECTED = 0,
     STATUS_BLOCKED = -1
} PCConnection;
typedef struct {
     char name[64];
     char ip[20];
} DiscoveredPC;
typedef struct
{
     char name[64];
     char user[30];
     char note[50];
     char ip[20];
     PCConnection status;
     PCState state;
     time_t last_seen;
     int64_t startedAt;
     int64_t untilTimestamp;
     int64_t runtime;
     char pending_cmd[16];
} PC;
time_t getRemainingTime(PC pc);
int64_t getTimestamp();
int insert_pc(PC* list, int* count, int max_capacity, int index, PC newItem);
void remove_pc(PC* list, int* count, int index);
void remove_all_pc(PC* list, int* count);

// Scanner
int scanClients(DiscoveredPC* results, int max_cap, int timeout_sec, PC* current_list, int current_count);

#endif