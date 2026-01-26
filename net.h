#ifndef __DIMPLOMA_NET_H
#define __DIMPLOMA_NET_H

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct sockaddr_in net_sock_addr;

static int lastError = 0;

#define READ_BUFFER_SIZE 256
static void* readBuffer;

int make_client();
int make_server_on_port(int port);
int send_to_address(int socket, const char* ip, int port);
int recv_packet(int socket, net_sock_addr* addr);
void* get_read_buffer();
void describe_address(net_sock_addr* addr);

int get_last_error();

#endif
