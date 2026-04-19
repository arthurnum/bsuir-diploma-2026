#include <string.h>

#include "s_conn_map.h"

SConnectionMap* make_conn_map() {
    SConnectionMap* map = calloc(1, sizeof(SConnectionMap));
    map->size = 0;
    map->capacity = 16;
    map->entries = calloc(map->capacity, sizeof(SConnection));
    return map;
}

uint16_t map_new_entry(SConnectionMap* map) {
    uint16_t idx = map->size;
    map->size++;
    return idx;
}
