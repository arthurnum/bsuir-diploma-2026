#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "../shared/net.h"
#include "../shared/protocol.h"
#include "s_conn_map.h"
#include "session_call.h"

static int server;

void retransmitFramePacket(SConnectionMap* connMap, SessionMap* sessionMap, uint16_t idx, uint8_t* data) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [32bit]
    // [7] data size [32bit]
    // [11] frame EOF flag [8bit]
    // [12] chunk number [16bit]
    // [14] frame ID [32bit]
    // [18] data [512B]
    uint16_t sessionId = connMap->entries[idx].session_id;
    SessionCall* session = &sessionMap->entries[sessionId];
    for (uint16_t i = 0; i < session->size; i++) {
        uint16_t connIdx = session->participantsIdx[i];
        if (connIdx != idx) {
            send_to_bin(server, connMap->entries[connIdx].addr, data, FRAME_PACKET_SIZE);
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

void sendUserList(SConnectionMap* connMap, uint8_t idx) {
    // [0] OPT code [8bit]
    // [1] list size [16bit]
    // [3] offset [16bit]
    // [5] count [16bit]
    // [7] data
    uint8_t* data = calloc(1, READ_BUFFER_SIZE);
    data[0] = PROTOCOL_USER_LIST;
    put_uint16_i(data, 1, connMap->size);

    uint8_t* listBuf;
    uint16_t count = 0;
    uint16_t i = 0;
    while (i < connMap->size) {
        put_uint16_i(data, 3, i); // offset

        if (i + 10 < connMap->size) {
            put_uint16_i(data, 5, 10); // count
            listBuf = calloc(1, 10 * USERNAME_ENTRY_SIZE);

            for (int j = 0; j < 10; j++) {
                put_uint16_i(listBuf, j * USERNAME_ENTRY_SIZE, i + j);
                memcpy(&listBuf[j * USERNAME_ENTRY_SIZE + 2], connMap->entries[i + j].username, USERNAME_SIZE);
            }
            i += 10;

            memcpy(&data[7], listBuf, 10 * USERNAME_ENTRY_SIZE);
            free(listBuf);
            send_to_bin(server, connMap->entries[idx].addr, data, READ_BUFFER_SIZE);
        } else {
            count = connMap->size - i;
            put_uint16_i(data, 5, count); // count
            listBuf = calloc(1, count * USERNAME_ENTRY_SIZE);

            for (int j = 0; j < count; j++) {
                put_uint16_i(listBuf, j * USERNAME_ENTRY_SIZE, i + j);
                memcpy(&listBuf[j * USERNAME_ENTRY_SIZE + 2], connMap->entries[i + j].username, USERNAME_SIZE);
            }
            i += count;

            memcpy(&data[7], listBuf, count * USERNAME_ENTRY_SIZE);
            free(listBuf);
            send_to_bin(server, connMap->entries[idx].addr, data, READ_BUFFER_SIZE);
        }
    }

    free(data);
}


int main() {
    int size;
    int max = 0;
    server = make_server_on_port(44323);
    printf("Socket FD %d\n", server);

    SConnectionMap* connMap = make_conn_map();
    SessionMap* sessionMap = make_session_map();

    net_sock_addr* a = calloc(1, sizeof(net_sock_addr));

    while (1) {
        size = recv_packet(server, a);
        uint8_t* data = get_read_buffer();

        uint16_t idx;
        uint16_t destIdx;
        uint16_t sessionIdx;
        uint8_t* bufResponse = calloc(1, 64);
        uint8_t opCode = data[0];
        uint8_t eof = 0;

        SConnection* conn = NULL;

        switch (opCode) {
            case PROTOCOL_NEW_CONNECTION:
                idx = map_new_entry(connMap);
                conn = &connMap->entries[idx];
                conn->username = calloc(USERNAME_SIZE, 1);
                memcpy(conn->username, &data[1], USERNAME_SIZE);
                conn->meta_str = describe_address(a);
                conn->addr = calloc(1, sizeof(net_sock_addr));
                memcpy(conn->addr, a, sizeof(net_sock_addr));
                printf("New connection: %s\n", conn->meta_str);
                printf("\tusername: %s\n", &data[1]);

                bufResponse[0] = PROTOCOL_ASSIGN_CONNECTION_IDX;
                put_uint16_i(bufResponse, 1, idx);
                memcpy(&bufResponse[3], conn->meta_str, 32);
                send_to_bin(server, conn->addr, bufResponse, 64);

                for (int i = 0; i < connMap->size; i++) {
                    printf("Send user list to %s\n", connMap->entries[i].meta_str);
                    sendUserList(connMap, i);
                }
                break;

            case PROTOCOL_CALL_REQUEST:
                idx = get_uint16_i(data, 1);
                destIdx = get_uint16_i(data, 3); // callee
                printf("%s calls %s\n", connMap->entries[idx].username, connMap->entries[destIdx].username);
                if (connMap->entries[destIdx].is_on_call) {
                    bufResponse[0] = PROTOCOL_USER_BUSY;
                    put_uint16_i(bufResponse, 1, destIdx);
                    send_to_bin(server, connMap->entries[idx].addr, bufResponse, PROTOCOL_USER_BUSY_SIZE);
                } else {
                    send_to_bin(server, connMap->entries[destIdx].addr, data, PROTOCOL_CALL_REQUEST_SIZE);
                }
                break;

            case PROTOCOL_CALL_CANCEL:
                idx = get_uint16_i(data, 1);
                destIdx = get_uint16_i(data, 3); // callee
                printf("%s cancel call to %s\n", connMap->entries[idx].username, connMap->entries[destIdx].username);
                send_to_bin(server, connMap->entries[destIdx].addr, data, PROTOCOL_CALL_CANCEL_SIZE);
                break;

            case PROTOCOL_CALL_REJECT:
                idx = get_uint16_i(data, 1);
                destIdx = get_uint16_i(data, 3); // caller
                printf("%s reject call from %s\n", connMap->entries[idx].username, connMap->entries[destIdx].username);
                send_to_bin(server, connMap->entries[destIdx].addr, data, PROTOCOL_CALL_REJECT_SIZE);
                break;

            case PROTOCOL_CALL_ACCEPT:
                idx = get_uint16_i(data, 1);
                destIdx = get_uint16_i(data, 3); // caller
                printf("%s accept call from %s\n", connMap->entries[idx].username, connMap->entries[destIdx].username);
                send_to_bin(server, connMap->entries[destIdx].addr, data, PROTOCOL_CALL_ACCEPT_SIZE);

                sessionIdx = open_session(sessionMap);
                addParticipant(&sessionMap->entries[sessionIdx], idx);
                addParticipant(&sessionMap->entries[sessionIdx], destIdx);
                connMap->entries[idx].session_id = sessionIdx;
                connMap->entries[idx].is_on_call = 1;
                connMap->entries[destIdx].session_id = sessionIdx;
                connMap->entries[destIdx].is_on_call = 1;
                break;

            case PROTOCOL_FRAME:
                idx = get_uint16_i(data, 1);
                retransmitFramePacket(connMap, sessionMap, idx, data);
                break;

            case PROTOCOL_FRAME_AUDIO:
                idx = get_uint16_i(data, 1);
                retransmitAudioFramePacket(connMap, idx, data);
                break;

            default:
                break;
        }

        conn = NULL;
        free(bufResponse);
    }

    close(server);
    return 0;
}
