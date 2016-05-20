#include <fcntl.h>
#include <memory.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>

#include "world.h"

#define MAX_SUPPORTED_MAP_VERSION 156

TerrariaWorld *terraria_open_world(
        const char *world_path,
        __attribute__((unused)) TerrariaError *error) {
    TerrariaWorld *world = malloc(sizeof(TerrariaWorld));

    world->fd = open(world_path, O_RDONLY);
    if (world->fd == -1) {
        error = _terraria_make_strerror();
        return NULL;
    }

    struct stat world_file_stats;
    if (fstat(world->fd, &world_file_stats) == -1) {
        error = _terraria_make_perror("Failed to read world file stats");
        goto fail;
    }

    world->file_size = (size_t) world_file_stats.st_size;

    world->start = mmap(NULL, world->file_size, PROT_READ, MAP_SHARED,
                        world->fd, 0);
    if (world->start == MAP_FAILED) {
        error = _terraria_make_perror("Failed to mmap world file");
        goto fail;
    }

    int world_version = *(int32_t *) world->start;
    if (world_version < 0 || world_version > MAX_SUPPORTED_MAP_VERSION) {
        error = _terraria_make_errorf("Unsupported world version: %d",
                                      world_version);
        goto fail;
    }


    int section_list_offset = 16;

    if (world_version >= 135) {
        if (memcmp("relogic", world->start + 4, 7) != 0) {
            error = _terraria_make_error("Invalid world file (Bad magic)");
            goto fail;
        }

        uint8_t file_type = world->start[11];
        if (file_type != 2) {
            error = _terraria_make_errorf("Not a world file (file type %u)",
                                          file_type);
            goto fail;
        }

        section_list_offset += 8;
    }

    world->section_count = *(int16_t *) (world->start + section_list_offset);
    if (world->section_count < 1) {
        error = _terraria_make_error("Invalid world file (Bad section list)");
        goto fail;
    }

    world->sections = (int32_t *) (world->start + section_list_offset + 2);
    world->extra = world->start + section_list_offset + 8;

    world->info = world->start + world->sections[0];
    world->tiles = world->start + world->sections[1];

    return world;

    fail:

    close(world->fd);
    free(world);

    return NULL;
}

void terraria_close_world(TerrariaWorld *world) {
    munmap(world, world->file_size);
    close(world->fd);
}


int terraria_get_world_size(
        const TerrariaWorld *world,
        __attribute__((unused)) int *width,
        __attribute__((unused)) int *height,
        __attribute__((unused)) TerrariaError *error) {
    uint8_t *world_info = world->start + world->sections[0];
    uint8_t title_length = *world_info;

    int32_t *world_size = (int32_t *) (world_info + title_length + 21);

    *height = *world_size++;
    *width = *world_size;

    return 1;
}