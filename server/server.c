#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "../shared/net.h"
#include "../shared/protocol.h"
#include "s_conn_map.h"

static int server;

void sendFramePacket(net_sock_addr* addr, SConnectionMap* connMap, uint16_t idx) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [32bit]
    // [7] data size [32bit]
    // [11] frame EOF flag [8bit]
    // [12] data [1024B]
    uint8_t* data = malloc(READ_BUFFER_SIZE);
    data[0] = PROTOCOL_FRAME;
    SConnection* conn = &connMap->entries[idx];
    *(uint16_t*)(&data[1]) = idx;
    *(uint32_t*)(&data[3]) = conn->frame_size;

    int i = 0;
    while (i < conn->frame_size) {
        uint32_t dataSize = FRAME_CHUNK;
        uint8_t oef_flag = 0;
        if (i + FRAME_CHUNK > conn->frame_size) {
            dataSize = (uint32_t)(conn->frame_size - i);
            oef_flag = FRAME_FLAG_EOF;
        }
        *(uint32_t*)(&data[7]) = dataSize;
        data[11] = oef_flag;
        memcpy(&data[12], &conn->frame_buf[i], dataSize);
        i += FRAME_CHUNK;
        send_to_bin(server, addr, data, READ_BUFFER_SIZE);
    }
    free(data);
}

int main() {
    int size;
    int max = 0;
    server = make_server_on_port(44323);
    printf("Socket FD %d\n", server);

    SConnectionMap* connMap = make_conn_map();

    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));

    while (1) {
        size = recv_packet(server, a);
        uint8_t* data = get_read_buffer();

        uint16_t idx;
        uint8_t* bufResponse;
        uint8_t opCode = data[0];

        switch (opCode) {
            case PROTOCOL_NEW_CONNECTION:
                idx = map_new_entry(connMap);
                connMap->entries[idx].meta_str = describe_address(a);
                printf("New connection: %s", connMap->entries[idx].meta_str);

                bufResponse = calloc(1, 64);
                bufResponse[0] = PROTOCOL_ASSIGN_CONNECTION_IDX;
                *(uint16_t*)(&bufResponse[1]) = idx;
                memcpy(&bufResponse[3], connMap->entries[idx].meta_str, 32);
                send_to_bin(server, a, bufResponse, 64);
                break;

            case PROTOCOL_FRAME:
                idx = get_uint16_i(data, 1);
                fill_frame_buffer(connMap, idx, data);
                if (data[11]) {
                    sendFramePacket(a, connMap, idx);
                }
                break;

            case PROTOCOL_FRAME_AUDIO:
                idx = get_uint16_i(data, 1);
                send_to_bin(server, a, data, size);
                break;

            default:
                break;
        }
    }
    close(server);
    return 0;
}
