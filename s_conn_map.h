#ifndef __DIMPLOMA_S_CONN_MAP_H
#define __DIMPLOMA_S_CONN_MAP_H

#include <stdlib.h>

typedef struct SConnection {
    char* meta_str;
} SConnection;

typedef struct SConnectionMap {
    SConnection* entries;
    uint16_t size;
    uint16_t capacity;
} SConnectionMap;

SConnectionMap* make_conn_map();

uint16_t map_new_entry(SConnectionMap* map);

#endif
