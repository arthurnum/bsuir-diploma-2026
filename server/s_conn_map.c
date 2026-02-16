#include <string.h>

#include "s_conn_map.h"
#include "../shared/net.h"

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

void fill_frame_buffer(SConnectionMap* map, uint16_t idx, uint8_t* data) {
    SConnection* conn = &map->entries[idx];
    uint32_t frameSize = get_uint32_i(data, 3);
    uint32_t dataSize = get_uint32_i(data, 7);
    if (conn->frame_buf == NULL) {
        conn->frame_buf = malloc(1 << 19);
        conn->frame_buf_ptr = conn->frame_buf;
    }
    if (conn->is_frame_eof) {
        conn->frame_buf_ptr = conn->frame_buf;
        conn->is_frame_eof = 0;
    }
    conn->frame_size = frameSize;
    memcpy(conn->frame_buf_ptr, &data[12], dataSize);
    conn->frame_buf_ptr += dataSize;
    conn->is_frame_eof = data[11];
}
