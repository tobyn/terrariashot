#include <fcntl.h>
#include <memory.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>

#include "world.h"

#define MAX_SUPPORTED_WORLD_VERSION 156
#define MIN_SUPPORTED_WORLD_VERSION 88

TerrariaWorld *terraria_open_world(
        const char *world_path,
        TerrariaError **error) {
    TerrariaWorld *world = malloc(sizeof(TerrariaWorld));
    if (world == NULL) {
        *error = MALLOC_FAILED;
        return NULL;
    }

    world->fd = open(world_path, O_RDONLY);
    if (world->fd == -1) {
        *error = _terraria_make_perror("Couldn't open world file");
        goto clean_up_malloc;
    }

    struct stat world_file_stats;
    if (fstat(world->fd, &world_file_stats) == -1) {
        *error = _terraria_make_perror("Couldn't read world file metadata");
        goto clean_up_open;
    }

    world->file_size = (size_t) world_file_stats.st_size;

    world->start = mmap(NULL, world->file_size, PROT_READ, MAP_SHARED,
                        world->fd, 0);
    if (world->start == MAP_FAILED) {
        *error = _terraria_make_perror("Failed to mmap world file");
        goto clean_up_open;
    }

    int world_version = *(int32_t *) world->start;
    if (world_version < MIN_SUPPORTED_WORLD_VERSION) {
        *error = _terraria_make_errorf(
                "World too old (version %d < %d)",
                world_version, MIN_SUPPORTED_WORLD_VERSION);
        goto clean_up_mmap;
    }
    if (world_version > MAX_SUPPORTED_WORLD_VERSION) {
        *error = _terraria_make_errorf(
                "World too new (version %d > %d)",
                world_version, MAX_SUPPORTED_WORLD_VERSION);
        goto clean_up_mmap;
    }


    int section_list_offset = 16;

    if (world_version >= 135) {
        if (memcmp("relogic", world->start + 4, 7) != 0) {
            *error = _terraria_make_error("Invalid world file (Bad magic)");
            goto clean_up_mmap;
        }

        uint8_t file_type = world->start[11];
        if (file_type != 2) {
            *error = _terraria_make_errorf("Not a world file (File type %u)",
                                           file_type);
            goto clean_up_mmap;
        }

        section_list_offset += 8;
    }

    world->section_count = *(int16_t *) (world->start + section_list_offset);
    if (world->section_count < 1) {
        *error = _terraria_make_error("Invalid world file (Bad section list)");
        goto clean_up_mmap;
    }

    world->sections = (int32_t *) (world->start + section_list_offset + 2);
    world->extra = world->start + section_list_offset + 8;

    world->info = world->start + world->sections[0];
    world->tiles = world->start + world->sections[1];

    return world;

    clean_up_mmap:
    munmap(world, world->file_size);

    clean_up_open:
    close(world->fd);

    clean_up_malloc:
    free(world);

    return NULL;
}

void terraria_close_world(TerrariaWorld *world) {
    munmap(world, world->file_size);
    close(world->fd);
}


int terraria_get_world_size(
        const TerrariaWorld *world,
        int *width,
        int *height,
        TerrariaError **error) {
    uint8_t title_length = *world->info;
    unsigned int size_offset = title_length + 21;

    if (size_offset + 8 > world->file_size) {
        *error = _terraria_make_errorf(
                "File ended prematurely (Expected at least %d bytes, got %d)",
                size_offset + 8,
                world->file_size);

        return 0;
    }

    int32_t *world_size = (int32_t *) (world->info + size_offset);
    *height = *world_size++;
    *width = *world_size;

    return 1;
}