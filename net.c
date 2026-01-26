#include "net.h"
#include <stdio.h>
#include <string.h>

int make_socket() {
    return socket(AF_INET, SOCK_DGRAM, 0);
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

int send_to_address(int socket, const char* ip, int port) {
    net_sock_addr* addr = address_with_port(ip, port);
    return sendto(socket, "hello!", 7, MSG_DONTWAIT, (struct sockaddr*)addr, sizeof(net_sock_addr));
}

int recv_packet(int socket, net_sock_addr* addr) {
    if (!readBuffer) {
        readBuffer = calloc(1, READ_BUFFER_SIZE);
    } else {
        memset(readBuffer, 0, READ_BUFFER_SIZE);
    }
    socklen_t addrLen = sizeof(net_sock_addr);
    return recvfrom(socket, readBuffer, 256, 0, (struct sockaddr*)addr, &addrLen);
}

void describe_address(net_sock_addr* addr) {
    char* buf = calloc(1, 32);
    printf("%s:%d\n", inet_ntop(AF_INET, &addr->sin_addr, buf, 32), ntohs(addr->sin_port));
    free(buf);
}

void* get_read_buffer() { return readBuffer; }
int get_last_error() { return lastError; }
