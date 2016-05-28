#include <stdio.h>
#include <stdlib.h>

#include "tile.h"

static int read_tile(TerrariaTileCursor *cursor, TerrariaError **error) {
    uint8_t *start = cursor->world->start + cursor->file_offset;

    TerrariaWorldCursor world_cursor;
    world_cursor.world = cursor->world;
    world_cursor.position = start;

    unsigned int flags1;
    if (!_terraria_read_uint8(&world_cursor, &flags1, error))
        return 0;

    unsigned int flags2 = 0;
    unsigned int flags3 = 0;

    if (flags1 & 1) {
        printf("    Has flags2\n");

        if (!_terraria_read_uint8(&world_cursor, &flags2, error))
            return 0;

        if (flags2 & 1) {
            printf("    Has flags3\n");

            if (!_terraria_read_uint8(&world_cursor, &flags3, error))
                return 0;
        }
    }

    if (flags1 & 2) {
        printf("    Is active!\n");

        unsigned int type;
        if (!_terraria_read_uint8(&world_cursor, &type, error))
            return 0;

        if (flags1 & 0x20) {
            printf("    Reading additional type byte\n");

            unsigned int more_type;
            if (!_terraria_read_uint8(&world_cursor, &more_type, error))
                return 0;

            type |= more_type << 8;
        }

        printf("    Type: %u\n", type);

        unsigned int extra;
        if (!_terraria_get_extra(cursor->world, type, &extra, error))
            return 0;

        if (extra) {
            printf("    Has U/V\n");

            if (!_terraria_seek_forward(&world_cursor, 4, error))
                return 0;
        }

        if (flags3 & 0x8) {
            printf("    Has color\n");

            if (!_terraria_seek_forward(&world_cursor, 1, error))
                return 0;
        }
    }

    if (flags1 & 4) {
        printf("    Is wall\n");
        if (!_terraria_seek_forward(&world_cursor, 1, error))
            return 0;

        if (flags3 & 0x10) {
            printf("    Has wall color\n");

            if (!_terraria_seek_forward(&world_cursor, 1, error))
                return 0;
        }
    }

    if (flags1 & 0x18) {
        printf("    Is liquid\n");

        if (!_terraria_seek_forward(&world_cursor, 1, error))
            return 0;
    }

    unsigned int rle = 0;
    int rle_format = flags1 >> 6;
    if (rle_format == 1) {
        printf("    Has 1 byte RLE\n");

        if (!_terraria_read_uint8(&world_cursor, &rle, error))
            return 0;
    } else if (rle_format == 2) {
        printf("    Has 2 byte RLE\n");

        int signed_rle;
        if (!_terraria_read_int16(&world_cursor, &signed_rle, error))
            return 0;

        if (signed_rle < 0) {
            *error = _terraria_make_error("Invalid map file (Negative RLE)");
        }

        rle = (unsigned int) signed_rle;
    }

    cursor->tile.rle = rle;
    cursor->tile.size = (unsigned int) (world_cursor.position - start);

    return 1;
}

int terraria_seek_tile(
        const TerrariaWorld *world,
        const unsigned int tile_offset,
        TerrariaTileCursor *cursor,
        TerrariaError **error) {
    TerrariaWorldCursor world_cursor;
    if (!_terraria_get_section(world, 1, &world_cursor, error))
        return 0;

    cursor->world = world;
    cursor->file_offset = world_cursor.position - world->start;

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

        printf("Tile %u at offset %lu (%u bytes, RLE %u)\n    ", tile_index,
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
