#ifndef TILE_H
#define TILE_H

#include "world.h"

typedef struct _TerrariaTile {
    unsigned int size;
    unsigned int rle;
} TerrariaTile;

typedef struct _TerrariaTileCursor {
    TerrariaWorld *world;
    TerrariaTile tile;
    unsigned long file_offset;
    unsigned int rle_offset;
} TerrariaTileCursor;


int terraria_seek_tile(
        TerrariaWorld *world,
        const unsigned int tile_offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error);

int terraria_seek_next_tile(TerrariaTileCursor *cursor, TerrariaError **error);

#endif // TILE_H
