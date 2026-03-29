#include <stdio.h>
#include <string.h>
#include "../shared/net.h"
#include "../shared/protocol.h"
#include "s_conn_map.h"

#if defined(_WIN32) || defined(_WIN64)
    // close() is not available on Windows, use CLOSE_SOCKET macro
#else
    #include <unistd.h>
#endif

static int server;

void retransmitFramePacket(SConnectionMap* connMap, uint16_t idx, uint8_t* data) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [32bit]
    // [7] data size [32bit]
    // [11] frame EOF flag [8bit]
    // [12] chunk number [16bit]
    // [14] frame ID [32bit]
    // [18] data [512B]
    for (int connIndex = 0; connIndex < connMap->size; connIndex++) {
        if (connIndex != idx) {
            send_to_bin(server, connMap->entries[connIndex].addr, data, FRAME_PACKET_SIZE);
            // printf("Send frame to %s",connMap->entries[connIndex].meta_str);
        }
    }
}

void retransmitAudioFramePacket(SConnectionMap* connMap, uint16_t idx, uint8_t* data) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [16bit]
    // [5] data [<512B]
    for (int connIndex = 0; connIndex < connMap->size; connIndex++) {
        if (connIndex != idx) {
            send_to_bin(server, connMap->entries[connIndex].addr, data, FRAME_AUDIO_PACKET_SIZE);
            // printf("Send frame to %s",connMap->entries[connIndex].meta_str);
        }
    }
}


int main() {
    int size;
    int max = 0;

    // Initialize network (required for Windows)
    net_init();

    server = make_server_on_port(44323);
    printf("Socket FD %d\n", server);

    SConnectionMap* connMap = make_conn_map();

    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));

    while (1) {
        size = recv_packet(server, a);
        uint8_t* data = get_read_buffer();

        uint16_t idx;
        uint8_t* bufResponse = calloc(1, 64);
        uint8_t opCode = data[0];
        uint8_t eof = 0;

        SConnection* conn = NULL;

        switch (opCode) {
            case PROTOCOL_NEW_CONNECTION:
                idx = map_new_entry(connMap);
                conn = &connMap->entries[idx];
                conn->meta_str = describe_address(a);
                conn->addr = calloc(1, sizeof(net_sock_addr));
                memcpy(conn->addr, a, sizeof(net_sock_addr));
                printf("New connection: %s", conn->meta_str);

                bufResponse[0] = PROTOCOL_ASSIGN_CONNECTION_IDX;
                put_uint16_i(bufResponse,1, idx);
                memcpy(&bufResponse[3], conn->meta_str, 32);
                send_to_bin(server, conn->addr, bufResponse, 64);
                break;

            case PROTOCOL_FRAME:
                idx = get_uint16_i(data, 1);
                retransmitFramePacket(connMap, idx, data);
                break;

            case PROTOCOL_FRAME_AUDIO:
                idx = get_uint16_i(data, 1);
                retransmitAudioFramePacket(connMap, idx, data);
                // send_to_bin(server, a, data, size);
                break;

                default:
                break;
        }

        conn = NULL;
        free(bufResponse);
    }

    CLOSE_SOCKET(server);
    net_cleanup();
    return 0;
}
