#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "world.h"

void die(const char *message) {
    fputs(message, stderr);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

void die_usage(const char *cmd) {
    fprintf(stderr,
            "Usage: %s <world> <left> <top> <width> <height> [zoom]\n",
            cmd);
    fputs("  world  - path to your .wld file\n", stderr);
    fputs("  left   - left edge coordinate of capture area\n", stderr);
    fputs("  top    - top edge coordinate of capture area\n", stderr);
    fputs("  width  - width of capture area in blocks\n", stderr);
    fputs("  height - height of capture area in blocks\n", stderr);
    fputs("  zoom   - zoom level [1-5]\n", stderr);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 7)
        die_usage(argv[0]);

    char *world_path = argv[1];

    int capture_left = atoi(argv[2]);
    int capture_top = atoi(argv[3]);
    int capture_width = atoi(argv[4]);
    int capture_height = atoi(argv[5]);

    int zoom = argc == 7 ? atoi(argv[6]) : 1;
    if (zoom < 1 || zoom > 5)
        die(_terraria_make_errorf("Invalid zoom level (1 <= %d <= 5)", zoom));

    TerrariaError *error = NULL;
    TerrariaWorld *world = terraria_open_world(world_path, &error);
    if (world == NULL)
        die(error);

    unsigned int blocks_wide, blocks_tall;
    if (!terraria_get_world_size(world, &blocks_wide, &blocks_tall, &error))
        die(error);

    int max_x = blocks_wide / 2;
    int max_y = blocks_tall / 2;
    int max_width = max_x - capture_left;
    int max_height = max_y - capture_top;

    if (capture_left < -max_x || capture_left >= max_x)
        die(_terraria_make_errorf(
                "Invalid capture area (left = %d <= %d < %d)",
                -max_x, capture_left, max_x));

    if (capture_width < 1 || capture_width > max_width)
        die(_terraria_make_errorf(
                "Invalid capture area (width = %d < %d < %d)",
                0, capture_width, max_width));

    if (capture_top < -max_y || capture_top >= max_y)
        die(_terraria_make_errorf(
                "Invalid capture area (top = %d <= %d < %d)",
                -max_y, capture_top, max_y));

    if (capture_height < 1 || capture_height > max_height)
        die(_terraria_make_errorf(
                "Invalid capture area (height = %d < %d < %d)",
                0, capture_height, max_height));

    int scale = 1 << (5 - zoom);
    printf("Capturing %dx%d blocks (%d), %dx%d pixels (%d)\n",
           capture_width, capture_height, capture_width * capture_height,
           capture_width * scale, capture_height * scale,
           capture_width * capture_height * scale);

    unsigned int offset = (capture_left * blocks_tall) + capture_top;
    unsigned int end_offset = (capture_left + capture_width - 1) * blocks_tall +
                     capture_top + capture_height;

    TerrariaTileCursor cursor;
    if (!terraria_seek_tile(world, offset, &cursor, &error))
        die(error);

    int captured = 0;
    uint8_t *tiles = cursor.tile;
    uint8_t *tile;
    while (1) {
        captured++;
        tile = cursor.tile;

        offset++;

        if (offset == end_offset)
            break;

        if (!terraria_seek_next_tile(&cursor, &error))
            die(error);
    }

    printf("Read %d tiles (%d * %d)\n", captured, capture_width,
           capture_height);
    printf("Total bytes read: %lu\n", tile - tiles);

    terraria_close_world(world);

    return EXIT_SUCCESS;
}
