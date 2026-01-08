// Seam.c
#include "Seam.h"
#include <stdio.h>
#include <string.h>

void seam_init(void) {
}

void init_server_socket(int* server_sock) {
     if (*server_sock != -1) close(*server_sock);
     *server_sock = socket(AF_INET, SOCK_DGRAM, 0);
     int opt = 1;
     setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
     struct sockaddr_in addr = { 0 };
     addr.sin_family = AF_INET;
     addr.sin_port = htons(8888);
     addr.sin_addr.s_addr = INADDR_ANY;
     bind(*server_sock, (struct sockaddr*)&addr, sizeof(addr));
     // Non-blocking
     int flags = fcntl(*server_sock, F_GETFL, 0);
     fcntl(*server_sock, F_SETFL, flags | O_NONBLOCK);
}

int seam_create_socket(int port) {
     int sock = socket(AF_INET, SOCK_DGRAM, 0);
     if (sock < 0) {
          perror("[SEAM] Socket creation failed");
          return -1;
     }
     struct sockaddr_in addr = { 0 };
     addr.sin_family = AF_INET;
     addr.sin_port = htons(port);
     addr.sin_addr.s_addr = INADDR_ANY;
     if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
          perror("[SEAM] Bind failed");

     int broadcast = 1;
     setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
     return sock;
}

void seam_set_nonblocking(int sock) {
     int flags = fcntl(sock, F_GETFL, 0);
     fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void seam_transmit(int sock, char* my_name) {
     SeamPacket pkt = { 0 };
     pkt.type = PKT_HELLO;
     strncpy(pkt.pc_name, my_name, 31);
     struct sockaddr_in dest = { 0 };
     dest.sin_family = AF_INET;
     dest.sin_port = htons(SEAM_SERVER_PORT);
     dest.sin_addr.s_addr = inet_addr(SEAM_BROADCAST_IP);
     sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)&dest, sizeof(dest));
}

void seam_send_heartbeat(int sock, struct sockaddr_in* server_addr, char* my_name) {
     SeamPacket pkt = { 0 };
     pkt.type = PKT_HEARTBEAT;
     strncpy(pkt.pc_name, my_name, 31);

     sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
}

void seam_send_sync(int sock, char* target_ip, char* pc_name, int state, int64_t end_time) {
     SeamPacket pkt = { 0 };
     pkt.type = PKT_SYNC;
     strncpy(pkt.pc_name, pc_name, 31);
     pkt.state = state;
     pkt.session_end = end_time;
     struct sockaddr_in dest = { 0 };
     dest.sin_family = AF_INET;
     dest.sin_port = htons(SEAM_CLIENT_PORT);
     dest.sin_addr.s_addr = inet_addr(target_ip);
     sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)&dest, sizeof(dest));
}

int seam_receive(int sock, SeamPacket* out_pkt, struct sockaddr_in* out_sender) {
     socklen_t len = sizeof(struct sockaddr_in);
     int n = recvfrom(sock, out_pkt, sizeof(SeamPacket), 0, (struct sockaddr*)out_sender, &len);
     if (n > 0) return 1;
     return 0;
}
