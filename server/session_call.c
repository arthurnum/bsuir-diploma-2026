#include "session_call.h"

void addParticipant(SessionCall* session, uint16_t idx) {
    uint16_t i = session->size;
    session->size++;
    session->participantsIdx = realloc(session->participantsIdx, sizeof(uint16_t) * session->size);
    session->participantsIdx[i] = idx;
}

SessionMap* make_session_map() {
    SessionMap* session = calloc(1, sizeof(SessionMap));
    session->size = 0;
    session->capacity = 16;
    session->entries = calloc(session->capacity, sizeof(SessionCall));
    return session;
}

uint16_t open_session(SessionMap* session) {
    uint16_t idx = session->size;
    session->size++;
    return idx;
}
