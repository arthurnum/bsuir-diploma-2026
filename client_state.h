#ifndef __DIPLOMA_CLIENT_STATE_H
#define __DIPLOMA_CLIENT_STATE_H

typedef struct ClientState {
    char camera_on;
    char mic_on;
    char next_frame_ready;
} ClientState;

#endif
