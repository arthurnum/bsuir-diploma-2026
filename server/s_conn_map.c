#include <string.h>

#include "s_conn_map.h"

SConnectionMap* make_conn_map() {
    SConnectionMap* map = malloc(sizeof(SConnectionMap));
    map->size = 0;
    map->capacity = 16;
    map->entries = malloc(sizeof(SConnection) * map->capacity);
    return map;
}

uint16_t map_new_entry(SConnectionMap* map) {
    uint16_t idx = map->size;
    map->size++;
    return idx;
}

int fill_frame_buffer(SConnectionMap* map, uint16_t idx, uint8_t* data) {
    SConnection* conn = &map->entries[idx];
    uint32_t frameSize = get_uint32_i(data, 3);
    uint32_t dataSize = get_uint32_i(data, 7);
    uint8_t eof = data[11];
    uint16_t chunkNumber = get_uint16_i(data, 12);
    uint32_t frameId = get_uint32_i(data, 14);

    if (conn->frame_buf == NULL) {
        conn->frame_buf = calloc(1 << 19, 1);
    }

    conn->current_frame_id = frameId;
    conn->frame_buf_ptr = conn->frame_buf + (FRAME_CHUNK * chunkNumber);
    memcpy(conn->frame_buf_ptr, &data[18], dataSize);
    conn->frame_size = frameSize;

    conn->next_chunk_number++;

    return eof;
}
