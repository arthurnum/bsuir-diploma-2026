#ifndef __DIPLOMA_CLIENT_STATE_H
#define __DIPLOMA_CLIENT_STATE_H

#include "user_list.h"

typedef struct ClientState {
    char camera_on;
    char mic_on;
    char next_frame_ready;
    char show_settings;
    char username_invalid;
    char on_call;
    UserList* users;
} ClientState;

#endif
