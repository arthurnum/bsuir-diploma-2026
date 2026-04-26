#ifndef __DIPLOMA_CLIENT_STATE_H
#define __DIPLOMA_CLIENT_STATE_H

#include "user_list.h"

typedef struct ClientState {
    char camera_on;
    char mic_on;
    char next_frame_ready;
    char show_settings;
    char show_users_list;
    char username_invalid;

    char waiting_call_response;
    char incoming_call;
    char on_call;
    char frame_seq_ready; // wait till next I-frame

    char show_user_busy;

    uint16_t connection_idx;
    uint16_t caller_idx;
    uint16_t callee_idx;
    UserList* users;

    int window_width;
    int window_height;
} ClientState;

#endif
