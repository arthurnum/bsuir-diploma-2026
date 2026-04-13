#ifndef __DIPLOMA_S_CONN_MAP_H
#define __DIPLOMA_S_CONN_MAP_H

#include <stdlib.h>
#include <stdint.h>
#include "../shared/net.h"

typedef struct SConnection {
    char* username;
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

#endif
