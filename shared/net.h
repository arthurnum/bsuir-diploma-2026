#ifndef __DIPLOMA_NET_H
#define __DIPLOMA_NET_H

#include <stdlib.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
    // Windows
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef struct sockaddr_in net_sock_addr;
    #define CLOSE_SOCKET closesocket
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define GET_SOCKET_ERROR() WSAGetLastError()
#else
    // Unix/Linux/macOS
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef struct sockaddr_in net_sock_addr;
    #define CLOSE_SOCKET close
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define GET_SOCKET_ERROR() errno
#endif

static int lastError = 0;

#define READ_BUFFER_SIZE 1024
static void* readBuffer;

#define FRAME_PACKET_SIZE 530 // 512 data + 18 header
#define FRAME_CHUNK 512

// Audio packet does not exceed 512 bytes
// add 5 bytes for header
#define FRAME_AUDIO_PACKET_SIZE 517

int make_client();
int make_server_on_port(int port);

// Cross-platform socket initialization/cleanup
void net_init();
void net_cleanup();

int send_to_bin(int socket, net_sock_addr* addr, uint8_t* data, int size);
int recv_packet(int socket, net_sock_addr* addr);
int recv_packet_dontwait(int socket);
int recv_packet_dontwait_peek(int socket);

net_sock_addr* address_with_port(const char* ip, int port);

void* get_read_buffer();
char* describe_address(net_sock_addr* addr);

int get_last_error();

uint16_t get_uint16_i(uint8_t* data, int i);
uint32_t get_uint32_i(uint8_t* data, int i);
void put_uint16_i(uint8_t* data, int i, uint16_t value);
void put_uint32_i(uint8_t* data, int i, uint32_t value);

#endif
