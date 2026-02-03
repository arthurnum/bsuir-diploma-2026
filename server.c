#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "net.h"
#include "protocol.h"
#include "s_conn_map.h"

int main() {
    int size;
    int max = 0;
    int server = make_server_on_port(44323);
    printf("Socket FD %d\n", server);

    SConnectionMap* connMap = make_conn_map();

    // int client = make_client();
    // printf("Client FD %d\n", client);

    // int size = send_to_address(client, "127.0.0.1", 44323);
    // printf("Client sent %d bytes\n", size);

    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));

    while (1) {
        size = recv_packet(server, a);
        uint8_t* data = get_read_buffer();

        uint16_t idx;
        uint8_t* bufResponse;
        uint8_t opCode = data[0];

        uint16_t testI = 1025;

        switch (opCode) {
            case PROTOCOL_NEW_CONNECTION:
                idx = map_new_entry(connMap);
                connMap->entries[idx].meta_str = describe_address(a);
                printf("New connection: %s", connMap->entries[idx].meta_str);

                bufResponse = calloc(1, 64);
                bufResponse[0] = PROTOCOL_ASSIGN_CONNECTION_IDX;
                *(uint16_t*)(&bufResponse[1]) = testI;
                memcpy(&bufResponse[3], connMap->entries[idx].meta_str, 32);
                send_to_bin(server, a, bufResponse, 64);
                break;

            default:
                break;
        }
        // printf("Server received %d bytes\n", size);
        // describe_address(a);
        // printf("Message %s\n", (char*)get_read_buffer());
    }


    // close(client);
    close(server);
    return 0;
}
