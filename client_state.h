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

    char incoming_call;
    char on_call;

    uint16_t caller_idx;
    UserList* users;
} ClientState;

#endif
