#ifndef WORLD_H
#define WORLD_H

#include "error.h"

typedef struct _TerrariaWorld {
    int fd;
    size_t file_size;
    int section_count;
    int32_t *sections;
    uint8_t *start, *extra, *info, *tiles;
} TerrariaWorld;

TerrariaWorld *terraria_open_world(const char *world_path, TerrariaError *error);
void terraria_close_world(TerrariaWorld *world);

int terraria_get_world_size(const TerrariaWorld *world, int *width, int *height, TerrariaError *error);

#endif // WORLD_H
