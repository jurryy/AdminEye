// Seam.h
#ifndef SEAM_H
#define SEAM_H

#include "PC.h"
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdlib.h>

// COFIG IMPORTANTE
#define SEAM_SERVER_PORT 8888
#define SEAM_CLIENT_PORT 9999
#define SEAM_BROADCAST_IP "255.255.255.255"

typedef enum {
     PKT_HELLO = 1,
     PKT_SYNC = 2,
     PKT_HEARTBEAT = 3
} PacketType;
typedef struct {
     int32_t type;
     char pc_name[32];
     int32_t state;
     int64_t session_end;
} __attribute__((packed)) SeamPacket;

void seam_init(void);
void init_server_socket(int* server_sock);
int seam_create_socket(int port);
void seam_set_nonblocking(int sock);

// Senders
void seam_transmit(int sock, char* my_name);
void seam_send_heartbeat(int sock, struct sockaddr_in* server_addr, char* my_name);
void seam_send_sync(int sock, char* target_ip, char* pc_name, int state, int64_t end_time);

// Receiver
int seam_receive(int sock, SeamPacket* out_pkt, struct sockaddr_in* out_sender);

#endif