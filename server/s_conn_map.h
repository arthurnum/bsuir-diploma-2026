#ifndef __DIMPLOMA_S_CONN_MAP_H
#define __DIMPLOMA_S_CONN_MAP_H

#include <stdlib.h>

typedef struct SConnection {
    uint8_t* frame_buf;
    uint8_t* frame_buf_ptr;
    uint32_t frame_size;
    uint8_t is_frame_eof;
    char* meta_str;
} SConnection;

typedef struct SConnectionMap {
    SConnection* entries;
    uint16_t size;
    uint16_t capacity;
} SConnectionMap;

SConnectionMap* make_conn_map();

uint16_t map_new_entry(SConnectionMap* map);
void fill_frame_buffer(SConnectionMap* map, uint16_t idx, uint8_t* data);

#endif
