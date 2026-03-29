#include "net.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    static WSADATA wsaData;
#else
    #include <fcntl.h>
    #include <errno.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
    static WSADATA wsaData;
#endif

void net_init() {
#if defined(_WIN32) || defined(_WIN64)
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        exit(1);
    }
#endif
}

void net_cleanup() {
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}

int make_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#if defined(_WIN32) || defined(_WIN64)
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        return -1;
    }
#else
    if (sock < 0) {
        perror("socket failed");
        return -1;
    }
#endif

    // Set non-blocking mode
#if defined(_WIN32) || defined(_WIN64)
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    return sock;
}

net_sock_addr* address_local_port(int port) {
    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));
    a->sin_family = AF_INET;
    a->sin_addr.s_addr = INADDR_ANY;
    a->sin_port = htons(port);
    return a;
}

net_sock_addr* address_with_port(const char* ip, int port) {
    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));
    a->sin_family = AF_INET;
    inet_pton(AF_INET, ip, &a->sin_addr);
    a->sin_port = htons(port);
    return a;
}

int make_client() {
    int socket = make_socket();
    return socket;
}

int make_server_on_port(int port) {
    int socket = make_socket();
    net_sock_addr* addr = address_local_port(port);
    lastError = bind(socket, (struct sockaddr*)addr, sizeof(net_sock_addr));
    if (lastError < 0) {
        perror("Could not bind socket: ");
    }
    return socket;
}

int send_to_bin(int socket, net_sock_addr* addr, uint8_t* data, int size) {
    // net_sock_addr* addr = address_with_port(ip, port);
#if defined(_WIN32) || defined(_WIN64)
    return sendto(socket, (const char*)data, size, 0, (struct sockaddr*)addr, sizeof(net_sock_addr));
#else
    return sendto(socket, data, size, MSG_DONTWAIT, (struct sockaddr*)addr, sizeof(net_sock_addr));
#endif
}

int recv_packet(int socket, net_sock_addr* addr) {
    if (!readBuffer) {
        readBuffer = calloc(1, READ_BUFFER_SIZE);
    } else {
        memset(readBuffer, 0, READ_BUFFER_SIZE);
    }
#if defined(_WIN32) || defined(_WIN64)
    int addrLen = sizeof(net_sock_addr);
    return recvfrom(socket, (char*)readBuffer, READ_BUFFER_SIZE, 0, (struct sockaddr*)addr, &addrLen);
#else
    socklen_t addrLen = sizeof(net_sock_addr);
    return recvfrom(socket, readBuffer, READ_BUFFER_SIZE, 0, (struct sockaddr*)addr, &addrLen);
#endif
}

int recv_packet_dontwait(int socket) {
    if (!readBuffer) {
        readBuffer = calloc(1, READ_BUFFER_SIZE);
    } else {
        memset(readBuffer, 0, READ_BUFFER_SIZE);
    }
#if defined(_WIN32) || defined(_WIN64)
    return recv(socket, (char*)readBuffer, READ_BUFFER_SIZE, 0);
#else
    return recv(socket, readBuffer, READ_BUFFER_SIZE, MSG_DONTWAIT);
#endif
}

int recv_packet_dontwait_peek(int socket) {
    if (!readBuffer) {
        readBuffer = calloc(1, READ_BUFFER_SIZE);
    } else {
        memset(readBuffer, 0, READ_BUFFER_SIZE);
    }
#if defined(_WIN32) || defined(_WIN64)
    return recv(socket, (char*)readBuffer, READ_BUFFER_SIZE, MSG_PEEK);
#else
    return recv(socket, readBuffer, READ_BUFFER_SIZE, MSG_DONTWAIT | MSG_PEEK);
#endif
}

char* describe_address(net_sock_addr* addr) {
    char* buf = calloc(1, 32);
    sprintf(buf, "%s:%d\n", inet_ntop(AF_INET, &addr->sin_addr, buf, 32), ntohs(addr->sin_port));
    return buf;
}

void* get_read_buffer() { return readBuffer; }
int get_last_error() { return lastError; }

uint16_t get_uint16_i(uint8_t* data, int i) {
    uint16_t netValue;
    memcpy(&netValue, &data[i], sizeof(netValue));
    return ntohs(netValue);
}

uint32_t get_uint32_i(uint8_t* data, int i) {
    uint32_t netValue;
    memcpy(&netValue, &data[i], sizeof(netValue));
    return ntohl(netValue);
}

void put_uint16_i(uint8_t* data, int i, uint16_t value) {
    uint16_t netValue = htons(value);
    memcpy(&data[i], &netValue, sizeof(netValue));
}

void put_uint32_i(uint8_t* data, int i, uint32_t value) {
    uint32_t netValue = htonl(value);
    memcpy(&data[i], &netValue, sizeof(netValue));
}
