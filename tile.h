#ifndef TILE_H
#define TILE_H

#include "world.h"

typedef struct _TerrariaTile {
    unsigned int size;
    unsigned int rle;
} TerrariaTile;

typedef struct _TerrariaTileCursor {
    const TerrariaWorld *world;
    TerrariaTile tile;
    unsigned int file_offset, rle_offset;
} TerrariaTileCursor;


int terraria_seek_tile(
        const TerrariaWorld *world,
        const unsigned int tile_offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error);

int terraria_seek_next_tile(TerrariaTileCursor *cursor, TerrariaError **error);

#endif // TILE_H
