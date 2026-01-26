#include <stdio.h>
#include <unistd.h>
#include "net.h"

int main() {
    int server = make_server_on_port(44323);
    printf("Socket FD %d\n", server);

    int client = make_client();
    printf("Client FD %d\n", client);

    int size = send_to_address(client, "127.0.0.1", 44323);
    printf("Client sent %d bytes\n", size);

    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));
    size = recv_packet(server, a);
    printf("Server received %d bytes\n", size);
    describe_address(a);
    printf("Message %s\n", (char*)get_read_buffer());


    close(client);
    close(server);
    return 0;
}
