#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#include "error.h"

typedef struct _TerrariaWorld {
    int fd;
    size_t file_size;
    uint8_t *start;
    unsigned int section_count;
    int32_t *sections;
    unsigned int extra_count;
    uint8_t *extra;
} TerrariaWorld;

typedef struct _TerrariaWorldCursor {
    const TerrariaWorld *world;
    uint8_t *position;
} TerrariaWorldCursor;

TerrariaWorld *terraria_open_world(
        const char *world_path,
        TerrariaError **error);

void terraria_close_world(TerrariaWorld *world);

int terraria_get_world_size(
        const TerrariaWorld *world,
        unsigned int *width,
        unsigned int *height,
        TerrariaError **error);

#endif // WORLD_H
