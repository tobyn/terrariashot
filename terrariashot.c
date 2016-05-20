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

    int blocks_wide, blocks_tall;
    if (!terraria_get_world_size(world, &blocks_wide, &blocks_tall, &error))
        die(error);

    int max_x = blocks_wide / 2;
    int max_y = blocks_tall / 2;

    int max_width = max_x - capture_left;
    int max_height = max_y - capture_top;

    if (capture_left < -max_x || capture_left >= max_x)
        die(_terraria_make_errorf("Invalid capture area (left = %d <= %d < %d)",
                                  -max_x, capture_left, max_x));

    if (capture_width < 1 || capture_width > max_width)
        die(_terraria_make_errorf("Invalid capture area (width = %d < %d < %d)",
                                  0, capture_width, max_width));

    if (capture_top < -max_y || capture_top >= max_y)
        die(_terraria_make_errorf("Invalid capture area (top = %d <= %d < %d)",
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

    uint8_t *tiles = world->tiles;
    uint8_t *tile = tiles;

    int x = -max_x, y = -max_x;
    int capture_right = capture_left + capture_width;
    int capture_bottom = capture_top + capture_height;
    int captured = 0;
    int parsing = 1;
    while (parsing) {
        uint8_t flags1 = *tile++;
        uint8_t flags2 = 0;
        uint8_t flags3 = 0;

        int has_flags2 = flags1 & 1;
        if (has_flags2)
            flags2 = *tile++;

        int has_flags3 = flags2 & 1;
        if (has_flags3)
            flags3 = *tile++;

        int active = (flags1 & 2) >> 1;
        if (active) {
            int type = *tile++;

            if (flags1 & 0x20)
                tile++; // type |= *tile++ << 8;

            if (world->extra[type]) {
                tile += 2; // u
                tile += 2; // v
            }

            if (flags3 & 0x8)
                tile++; // color
        }

        if (flags1 & 4) {
            tile++; // wall

            if (flags3 & 0x10)
                tile++; // wall color
        }

        if (flags1 & 0x18)
            tile++; // liquid

        int rle = 0;
        int rle_format = flags1 >> 6;
        if (rle_format == 1) {
            rle = *tile++;
        } else if (rle_format == 2) {
            rle = *(int16_t *) tile;
            tile += 2;
        }

        while (rle-- >= 0) {
            if (x >= capture_left &&
                x < capture_right &&
                y >= capture_top &&
                y < capture_bottom)
                captured++;

            y++;

            if (y >= max_y) {
                y = -max_y;
                x++;
            }

            if (x + 1 == capture_right && y + 1 == capture_bottom) {
                parsing = 0;
                break;
            }
        }
    }

    printf("Read %d tiles (%d * %d)\n", captured, capture_width,
           capture_height);
    printf("Total bytes read: %lu\n", tile - tiles);

    terraria_close_world(world);

    return EXIT_SUCCESS;
}
