#ifndef WORLD_H
#define WORLD_H

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

typedef struct _TerrariaTileCursor {
    uint8_t *tile;
    unsigned int size, rle_offset;
} TerrariaTileCursor;

TerrariaWorld *terraria_open_world(
        const char *world_path,
        TerrariaError **error);

void terraria_close_world(TerrariaWorld *world);

int terraria_get_world_size(
        const TerrariaWorld *world,
        unsigned int *width,
        unsigned int *height,
        TerrariaError **error);

int terraria_seek_tile(
        const TerrariaWorld *world,
        const unsigned int offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error);

int terraria_seek_next_tile(
        TerrariaTileCursor *cursor,
        TerrariaError **error);

#endif // WORLD_H
