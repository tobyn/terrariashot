#include <stdio.h>
#include <stdlib.h>

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

unsigned int atoui(const char *a) {
    int i = atoi(a);
    if (i < 0)
        die(_terraria_make_errorf("Invalid unsigned integer: %s", a));

    return (unsigned int) i;
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 7)
        die_usage(argv[0]);

    char *world_path = argv[1];

    int capture_left = atoi(argv[2]);
    int capture_top = atoi(argv[3]);
    unsigned int capture_width = atoui(argv[4]);
    unsigned int capture_height = atoui(argv[5]);

    unsigned int zoom = argc == 7 ? atoui(argv[6]) : 1;
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
    unsigned int max_width = (unsigned int) max_x - capture_left;
    unsigned int max_height = (unsigned int) max_y - capture_top;

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

    unsigned int scale = (unsigned int) (1 << (5 - zoom));
    unsigned int capture_size = capture_width * capture_height;
    printf("Capturing %ux%u blocks (%u), %ux%u pixels (%u)\n",
           capture_width, capture_height, capture_size,
           capture_width * scale, capture_height * scale,
           capture_width * capture_height * scale);

    int left_offset = max_x + capture_left;
    int top_offset = max_y + capture_top;
    unsigned int offset = (left_offset * blocks_tall) + top_offset;

    TerrariaTileCursor cursor;
    if (!terraria_seek_tile(world, offset, &cursor, &error))
        die(error);

    unsigned int captured = 0;
    while (1) {
        captured++;

        if (captured == capture_size)
            break;

        if (!terraria_seek_next_tile(&cursor, &error))
            die(error);
    }

    printf("Read %u tiles (%u * %u)\n", captured, capture_width,
           capture_height);
    printf("Total bytes read: %u\n", cursor.file_offset + cursor.tile.size);

    terraria_close_world(world);

    return EXIT_SUCCESS;
}
