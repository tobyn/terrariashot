#include <stdio.h>
#include <stdlib.h>

#include "tile.h"

static int read_tile(TerrariaTileCursor *cursor, TerrariaError **error) {
    uint8_t *bytes;

    unsigned int position = cursor->file_offset;
    bytes = get_bytes(cursor->world, position++, 1, error);
    if (bytes == NULL)
        return 0;

    uint8_t flags1 = *bytes;
    uint8_t flags2 = 0;
    uint8_t flags3 = 0;

    int has_flags2 = flags1 & 1;
    if (has_flags2) {
        printf("    Has flags2\n");
        bytes = get_bytes(cursor->world, position++, 1, error);
        if (bytes == NULL)
            return 0;

        flags2 = *bytes;


        int has_flags3 = flags2 & 1;
        if (has_flags3) {
            printf("    Has flags3\n");
            bytes = get_bytes(cursor->world, position++, 1, error);
            if (bytes == NULL)
                return 0;

            flags3 = *bytes;
        }
    }

    int active = (flags1 >> 1) & 1;
    if (active) {
        printf("    Is active!\n");
        bytes = get_bytes(cursor->world, position++, 1, error);
        if (bytes == NULL)
            return 0;

        int type = *bytes;

        if (flags1 & 0x20) {
            printf("    Reading extra\n");
            bytes = get_bytes(cursor->world, --position, 2, error);
            if (bytes == NULL)
                return 0;

            position += 2;
            type = *(uint16_t *) bytes;
        }

        printf("    Type: %u\n", type);

        bytes = get_extra(cursor->world, type, error);
        if (bytes == NULL)
            return 0;

        if (*bytes) {
            position += 4; // skip u and v (int16)
            printf("    Has U/V\n");
        }

        if (flags3 & 0x8) {
            printf("    Has color\n");
            position++; // color
        }
    }

    if (flags1 & 4) {
        printf("    Is wall\n");
        position++; // wall

        if (flags3 & 0x10) {
            printf("    Has wall color\n");
            position++; // wall color
        }
    }

    if (flags1 & 0x18) {
        position++; // liquid
        printf("    Is liquid\n");
    }

    int rle = 0;
    int rle_format = flags1 >> 6;
    if (rle_format == 1) {
        printf("    Has 1 byte RLE\n");

        bytes = get_bytes(cursor->world, position++, 1, error);
        if (bytes == NULL)
            return 0;

        rle = *bytes;
    } else if (rle_format == 2) {
        printf("    Has 2 byte RLE\n");

        bytes = get_bytes(cursor->world, position, 2, error);
        if (bytes == NULL)
            return 0;

        position += 2;
        rle = *(int16_t *) bytes;
    }

    if (rle < 0) {
        *error = _terraria_make_error("Invalid map file (Negative RLE)");
        return 0;
    }

    cursor->tile.rle = (unsigned int) rle;
    cursor->tile.size = position - cursor->file_offset;

    return 1;
}

int terraria_seek_tile(
        const TerrariaWorld *world,
        const unsigned int tile_offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error) {
    unsigned int file_offset;
    if (!get_section_offset(world, 1, &file_offset, error))
        return 0;

    cursor->world = world;
    cursor->file_offset = file_offset;

    unsigned int tile_index = 0;
    while (1) {
        if (!read_tile(cursor, error)) {
            printf("Read failed at tile offset %u: %s\n    ", tile_index, *error);

            uint8_t *byte = cursor->world->start + cursor->file_offset;
            for (unsigned int i = 0; i < cursor->tile.size; i++, byte++) {
                printf("%d%d%d%d%d%d%d%d ",
                       (*byte >> 7) & 1,
                       (*byte >> 6) & 1,
                       (*byte >> 5) & 1,
                       (*byte >> 4) & 1,
                       (*byte >> 3) & 1,
                       (*byte >> 2) & 1,
                       (*byte >> 1) & 1,
                       *byte & 1);
            }
            puts("");

            return 0;
        }

        printf("Tile %u at offset %u (%u bytes, RLE %u)\n    ", tile_index,
               cursor->file_offset, cursor->tile.size, cursor->tile.rle);

        uint8_t *byte = cursor->world->start + cursor->file_offset;
        for (unsigned int i = 0; i < cursor->tile.size; i++, byte++) {
            printf("%d%d%d%d%d%d%d%d ",
                   (*byte >> 7) & 1,
                   (*byte >> 6) & 1,
                   (*byte >> 5) & 1,
                   (*byte >> 4) & 1,
                   (*byte >> 3) & 1,
                   (*byte >> 2) & 1,
                   (*byte >> 1) & 1,
                   *byte & 1);
        }
        puts("");

        if (tile_index >= 200)
            exit(0);

        unsigned int last_index = tile_index + cursor->tile.rle;
        if (last_index >= tile_offset) {
            cursor->rle_offset = tile_offset - tile_index;
            return 1;
        } else {
            tile_index += 1 + cursor->tile.rle;
            cursor->file_offset += cursor->tile.size;
        }
    }
}

int terraria_seek_next_tile(TerrariaTileCursor *cursor, TerrariaError **error) {
    if (cursor->rle_offset < cursor->tile.rle) {
        cursor->rle_offset++;
        return 1;
    }

    cursor->file_offset += cursor->tile.size;
    cursor->rle_offset = 0;

    return read_tile(cursor, error);
}
