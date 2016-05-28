#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#include "error.h"

typedef struct _TerrariaWorld {
    int fd;
    unsigned int file_size;
    uint8_t *start;
    unsigned int section_count;
    int32_t *sections;
    unsigned int tile_extra_count;
    unsigned int *tile_extra;
} TerrariaWorld;

typedef struct _TerrariaWorldCursor {
    TerrariaWorld *world;
    uint8_t *position;
} TerrariaWorldCursor;

int terraria_open_world(
        const char *world_path,
        TerrariaWorld *world,
        TerrariaError **error);

void terraria_close_world(TerrariaWorld *world);

int terraria_get_world_size(
        TerrariaWorld *world,
        unsigned int *width,
        unsigned int *height,
        TerrariaError **error);

int _terraria_read_uint8(
        TerrariaWorldCursor *cursor,
        unsigned int *out,
        TerrariaError **error);

int _terraria_read_int16(
        TerrariaWorldCursor *cursor,
        int *out,
        TerrariaError **error);

int _terraria_read_int32(
        TerrariaWorldCursor *cursor,
        int *out,
        TerrariaError **error);

int _terraria_read_string(
        TerrariaWorldCursor *cursor,
        unsigned int *length,
        char **out,
        TerrariaError **error);

int _terraria_seek_forward(
        TerrariaWorldCursor *cursor,
        const unsigned int bytes,
        TerrariaError **error);

int _terraria_get_section(
        TerrariaWorld *world,
        const unsigned int section_offset,
        TerrariaWorldCursor *cursor,
        TerrariaError **error);

int _terraria_get_extra(
        TerrariaWorld *world,
        const unsigned int type,
        unsigned int *extra,
        TerrariaError **error);

#endif // WORLD_H
