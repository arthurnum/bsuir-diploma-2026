#ifndef __DIPLOMA_SESSION_CALL_H
#define __DIPLOMA_SESSION_CALL_H

#include <stdlib.h>
#include <stdint.h>

typedef struct SessionCall {
    uint16_t size;
    uint16_t* participantsIdx;
} SessionCall;

void addParticipant(SessionCall* session, uint16_t idx);

typedef struct SessionMap {
    SessionCall* entries;
    uint16_t size;
    uint16_t capacity;
} SessionMap;

SessionMap* make_session_map();
uint16_t open_session(SessionMap* session);

#endif
