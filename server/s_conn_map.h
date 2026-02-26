#ifndef __DIPLOMA_S_CONN_MAP_H
#define __DIPLOMA_S_CONN_MAP_H

#include <stdlib.h>
#include <stdint.h>
#include "../shared/net.h"

typedef struct SConnection {
    uint8_t* frame_buf;
    uint8_t* frame_buf_ptr;
    uint32_t frame_size;
    uint32_t current_frame_id;
    uint16_t next_chunk_number;
    uint8_t is_frame_eof;
    char* meta_str;
    net_sock_addr* addr;
} SConnection;

typedef struct SConnectionMap {
    SConnection* entries;
    uint16_t size;
    uint16_t capacity;
} SConnectionMap;

SConnectionMap* make_conn_map();

uint16_t map_new_entry(SConnectionMap* map);
int fill_frame_buffer(SConnectionMap* map, uint16_t idx, uint8_t* data);

#endif
