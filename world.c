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

static uint8_t *get_bytes(
        const TerrariaWorld *world,
        const unsigned int offset,
        const unsigned int size,
        TerrariaError **error) {
    if (offset < 0) {
        *error = _terraria_make_errorf(
                "Negative file offset requested (%d < 0)",
                offset);
        return NULL;
    }

    if (offset + size > world->file_size) {
        *error = _terraria_make_errorf(
                "World file ended prematurely (Expected at least %d bytes, got %d)",
                offset + size, world->file_size);
        return NULL;
    }

    return world->start + offset;
}

static unsigned int get_section_offset(
        const TerrariaWorld *world,
        const unsigned int section,
        unsigned int *offset,
        TerrariaError **error) {
    if (section >= world->section_count) {
        *error = _terraria_make_errorf(
                "Requested out of bounds section (%d >= %d)",
                section, world->section_count);
        return 0;
    }

    int signed_offset = world->sections[section];
    if (signed_offset < 0) {
        *error = _terraria_make_error(
                "Invalid world file (Negative section offset)");
        return 0;
    }

    *offset = (unsigned int) signed_offset;
    return 1;
}

static uint8_t *get_section(
        const TerrariaWorld *world,
        const unsigned int section,
        TerrariaError **error) {
    unsigned int offset;
    if (!get_section_offset(world, section, &offset, error))
        return NULL;

    return world->start + offset;
}

static uint8_t *get_extra(
        const TerrariaWorld *world,
        const unsigned int type,
        TerrariaError **error) {
    if (type >= world->extra_count) {
        *error = _terraria_make_errorf(
                "Requested out of bounds type (%d >= %d)",
                type, world->extra_count);
        return NULL;
    }

    return world->extra + type;
}

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

    uint8_t *bytes = get_bytes(world, 0, 4, error);
    if (bytes == NULL)
        goto clean_up_mmap;

    int world_version = *(int32_t *) bytes;
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

    unsigned int offset = 16;

    if (world_version >= 135) {
        bytes = get_bytes(world, 4, 11, error);
        if (bytes == NULL)
            goto clean_up_mmap;

        if (memcmp("relogic", bytes, 7) != 0) {
            *error = _terraria_make_error("Invalid world file (Bad magic)");
            goto clean_up_mmap;
        }

        unsigned int file_type = *(bytes + 7);
        if (file_type != 2) {
            *error = _terraria_make_errorf("Not a world file (File type %u)",
                                           file_type);
            goto clean_up_mmap;
        }

        offset += 8;
    }

    bytes = get_bytes(world, offset, 2, error);
    if (bytes == NULL)
        goto clean_up_mmap;

    int signed_count = *(int16_t *) bytes;
    if (signed_count < 1) {
        *error = _terraria_make_error("Invalid world file (Bad section list)");
        goto clean_up_mmap;
    }

    world->section_count = (unsigned int) signed_count;

    unsigned int section_list_size = 4 * world->section_count;

    offset += 2;
    bytes = get_bytes(world, offset, section_list_size + 2, error);
    if (bytes == NULL)
        goto clean_up_mmap;

    world->sections = (int32_t *) bytes;

    signed_count = *(int16_t *) (bytes + section_list_size);
    if (signed_count < 1) {
        *error = _terraria_make_error("Invalid world file (Bad extra list)");
        goto clean_up_mmap;
    }

    world->extra_count = (unsigned int) signed_count;

    offset += section_list_size + 2;
    world->extra = get_bytes(world, offset, world->extra_count, error);
    if (world->extra == NULL)
        goto clean_up_mmap;

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
        unsigned int *width,
        unsigned int *height,
        TerrariaError **error) {
    unsigned int header_offset;
    if (!get_section_offset(world, 0, &header_offset, error))
        return 0;

    uint8_t *bytes = get_bytes(world, header_offset, 1, error);
    if (bytes == NULL)
        return 0;

    uint8_t title_size = *bytes;
    unsigned int size_offset = header_offset + title_size + 21;

    bytes = get_bytes(world, size_offset, 8, error);
    if (bytes == NULL)
        return 0;

    int32_t *world_size = (int32_t *) bytes;

    int signed_size = *world_size++;
    if (signed_size < 0) {
        *error = _terraria_make_errorf("Invalid world height (%d)",
                                       signed_size);
        return 0;
    }

    *height = (unsigned int) signed_size;

    signed_size = *world_size;
    if (signed_size < 0) {
        *error = _terraria_make_errorf("Invalid world width (%d)", signed_size);
        return 0;
    }

    *width = (unsigned int) signed_size;

    return 1;
}

int terraria_seek_tile(
        const TerrariaWorld *world,
        const unsigned int offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error) {
    unsigned int position;
    if (!get_section_offset(world, 1, &position, error))
        return 0;

    uint8_t *bytes;
    unsigned int index = 0;
    while (index <= offset) {
        bytes = get_bytes(world, position++, 1, error);
        if (bytes == NULL)
            return 0;

        uint8_t flags1 = *bytes;
        uint8_t flags2 = 0;
        uint8_t flags3 = 0;

        int has_flags2 = flags1 & 1;
        if (has_flags2) {
            bytes = get_bytes(world, position++, 1, error);
            if (bytes == NULL)
                return 0;

            flags2 = *bytes;
        }

        int has_flags3 = flags2 & 1;
        if (has_flags3) {
            bytes = get_bytes(world, position++, 1, error);
            if (bytes == NULL)
                return 0;

            flags3 = *bytes;
        }

        int active = (flags1 >> 1) & 1;
        if (active) {
            bytes = get_bytes(world, position++, 1, error);
            if (bytes == NULL)
                return 0;

            unsigned int type = *bytes;

            if (flags1 & 0x20) {
                bytes = get_bytes(world, position++, 1, error);
                if (bytes == NULL)
                    return 0;

                type |= *bytes << 8;
            }

            bytes = get_extra(world, type, error);
            if (bytes == NULL)
                return 0;

            if (*bytes)
                position += 4; // skip u and v (int16)

            if (flags3 & 0x8)
                position++; // color
        }

        if (flags1 & 4) {
            position++; // wall

            if (flags3 & 0x10)
                position++; // wall color
        }

        if (flags1 & 0x18)
            position++; // liquid

        int rle = 0;
        int rle_format = flags1 >> 6;
        if (rle_format == 1) {
            bytes = get_bytes(world, position++, 1, error);
            if (bytes == NULL)
                return 0;

            rle = *bytes;
        } else if (rle_format == 2) {
            bytes = get_bytes(world, position, 2, error);
            if (bytes == NULL)
                return 0;

            position += 2;
            rle = *(int16_t *) bytes;
        }

        if (rle < 0) {
            *error = _terraria_make_error("Invalid map file (Negative RLE)");
            return 0;
        }

        index += rle;
    }

    cursor->tile = bytes;
    cursor->size =
}

int terraria_seek_next_tile(
        TerrariaTileCursor *cursor,
        TerrariaError **error) {

}