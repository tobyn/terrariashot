#include <fcntl.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>

#include "world.h"

#define MAX_SUPPORTED_WORLD_VERSION 156
#define MIN_SUPPORTED_WORLD_VERSION 88


static int require_bytes(
        const TerrariaWorldCursor *cursor,
        const unsigned int bytes,
        TerrariaError **error) {
    unsigned long start = cursor->position - cursor->world->start;

    if (start + bytes > cursor->world->file_size) {
        *error = _terraria_make_errorf(
                "World file ended prematurely (Expected at least %d bytes, got %d)",
                start + bytes, cursor->world->file_size);
        return 0;
    }

    return 1;
}


int terraria_open_world(
        const char *world_path,
        TerrariaWorld *world,
        TerrariaError **error) {
    world->fd = open(world_path, O_RDONLY);
    if (world->fd == -1) {
        *error = _terraria_make_perror("Couldn't open world file");
        return 0;
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

    TerrariaWorldCursor cursor;
    cursor.world = world;
    cursor.position = world->start;

    int world_version;
    if (!_terraria_read_int32(&cursor, &world_version, error))
        goto clean_up_mmap;

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

    if (world_version >= 135) {
        if (!_terraria_seek_forward(&cursor, 7, error))
            goto clean_up_mmap;

        if (memcmp("relogic", cursor.position - 7, 7) != 0) {
            *error = _terraria_make_error("Invalid world file (Bad magic)");
            goto clean_up_mmap;
        }

        unsigned int file_type;
        if (!_terraria_read_uint8(&cursor, &file_type, error))
            goto clean_up_mmap;

        if (file_type != 2) {
            *error = _terraria_make_errorf("Not a world file (File type %u)",
                                           file_type);
            goto clean_up_mmap;
        }

        if (!_terraria_seek_forward(&cursor, 8, error))
            goto clean_up_mmap;
    }

    if (!_terraria_seek_forward(&cursor, 16, error))
        goto clean_up_mmap;

    int signed_count;
    if (!_terraria_read_int16(&cursor, &signed_count, error))
        goto clean_up_mmap;

    if (signed_count < 1) {
        *error = _terraria_make_error("Invalid world file (Bad section list)");
        goto clean_up_mmap;
    }

    world->section_count = (unsigned int) signed_count;
    world->sections = (int32_t *) cursor.position;

    if (!_terraria_seek_forward(&cursor, 4 * world->section_count, error))
        goto clean_up_mmap;

    if (!_terraria_read_int16(&cursor, &signed_count, error))
        goto clean_up_mmap;

    if (signed_count < 1) {
        *error = _terraria_make_error("Invalid world file (Bad extra list)");
        goto clean_up_mmap;
    }

    world->extra_count = (unsigned int) signed_count;
    world->extra = cursor.position;

    if (!require_bytes(&cursor, world->extra_count, error))
        goto clean_up_mmap;

    return 1;

    clean_up_mmap:
    munmap(world, world->file_size);

    clean_up_open:
    close(world->fd);

    return 0;
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
    TerrariaWorldCursor cursor;
    if (!_terraria_get_section(world, 0, &cursor, error))
        return 0;

    unsigned int title_length;
    char *title;
    if (!_terraria_read_string(&cursor, &title_length, &title, error))
        return 0;

    if (!_terraria_seek_forward(&cursor, 21, error))
        return 0;

    int signed_width;
    if (!_terraria_read_int32(&cursor, &signed_width, error))
        return 0;

    if (signed_width <= 0) {
        *error = _terraria_make_errorf("Invalid world height (%d)",
                                       signed_width);
        return 0;
    }

    int signed_height;
    if (!_terraria_read_int32(&cursor, &signed_height, error))
        return 0;

    if (signed_height < 0) {
        *error = _terraria_make_errorf("Invalid world width (%d)",
                                       signed_height);
        return 0;
    }

    *width = (unsigned int) signed_width;
    *height = (unsigned int) signed_height;

    return 1;
}


int _terraria_read_uint8(
        TerrariaWorldCursor *cursor,
        unsigned int *out,
        TerrariaError **error) {
    if (!require_bytes(cursor, 1, error))
        return 0;

    *out = *cursor->position++;

    return 1;
}

int _terraria_read_int16(
        TerrariaWorldCursor *cursor,
        int *out,
        TerrariaError **error) {
    if (!require_bytes(cursor, 2, error))
        return 0;

    *out = *(int16_t *) cursor->position;
    cursor->position += 2;

    return 1;
}

int _terraria_read_int32(
        TerrariaWorldCursor *cursor,
        int *out,
        TerrariaError **error) {
    if (!require_bytes(cursor, 4, error))
        return 0;

    *out = *(int32_t *) cursor->position;
    cursor->position += 4;

    return 1;
}

int _terraria_read_string(
        TerrariaWorldCursor *cursor,
        unsigned int *length,
        char **out,
        TerrariaError **error) {
    if (!_terraria_read_uint8(cursor, length, error))
        return 0;

    *out = (char *) cursor->position;

    return _terraria_seek_forward(cursor, *length, error);
}

int _terraria_seek_forward(
        TerrariaWorldCursor *cursor,
        const unsigned int bytes,
        TerrariaError **error) {
    if (!require_bytes(cursor, bytes, error))
        return 0;

    cursor->position += bytes;

    return 1;
}


int _terraria_get_section(
        const TerrariaWorld *world,
        const unsigned int section_offset,
        TerrariaWorldCursor *cursor,
        TerrariaError **error) {
    if (section_offset >= world->section_count) {
        *error = _terraria_make_errorf(
                "Requested out of bounds section (%d >= %d)",
                section_offset, world->section_count);
        return 0;
    }

    int file_offset = world->sections[section_offset];
    if (file_offset < 0) {
        *error = _terraria_make_errorf(
                "Invalid world file (Negative file offset for section %d)",
                section_offset);
        return 0;
    }

    cursor->world = world;
    cursor->position = world->start + file_offset;

    return 1;
}

int _terraria_get_extra(
        const TerrariaWorld *world,
        const unsigned int type,
        unsigned int *extra,
        TerrariaError **error) {
    if (type >= world->extra_count) {
        *error = _terraria_make_errorf(
                "Requested out of bounds extra (%d >= %d)",
                type, world->extra_count);
        return 0;
    }

    *extra = *(world->extra + type);
    return 1;
}