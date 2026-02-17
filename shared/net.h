#ifndef __DIMPLOMA_NET_H
#define __DIMPLOMA_NET_H

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct sockaddr_in net_sock_addr;

static int lastError = 0;

#define READ_BUFFER_SIZE 1024
static void* readBuffer;

#define FRAME_PACKET_SIZE 526 // 512 data + 14 header
#define FRAME_CHUNK 512

int make_client();
int make_server_on_port(int port);

int send_to_bin(int socket, net_sock_addr* addr, uint8_t* data, int size);
int recv_packet(int socket, net_sock_addr* addr);
int recv_packet_dontwait(int socket);

net_sock_addr* address_with_port(const char* ip, int port);

void* get_read_buffer();
char* describe_address(net_sock_addr* addr);

int get_last_error();

uint16_t get_uint16_i(uint8_t* data, int i);
uint32_t get_uint32_i(uint8_t* data, int i);

#endif
