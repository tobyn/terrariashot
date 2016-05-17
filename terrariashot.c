#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_SUPPORTED_MAP_VERSION 156

char *cmd;

uint8_t *world;
char *world_path;
int world_fd;
size_t world_file_size;
int32_t world_version;

int16_t section_count;
int32_t *section_offsets;
uint8_t *extra;

void die(const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    fputs("\n", stderr);

    va_end(args);

    exit(EXIT_FAILURE);
}

void die_usage() {
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

void die_invalid(const char *reason) {
    die("Invalid world file (%s)", reason);
}

void die_perror(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void open_world(void) {
    world_fd = open(world_path, O_RDONLY);
    if (world_fd == -1)
        die_perror("Couldn't open world file");

    struct stat world_file_stats;
    if (fstat(world_fd, &world_file_stats) == -1)
        die_perror("Couldn't read world file stats");

    world_file_size = (size_t) world_file_stats.st_size;

    world = mmap(NULL, world_file_size, PROT_READ, MAP_SHARED, world_fd, 0);
    if (world == MAP_FAILED)
        die_perror("Couldn't mmap world file");

    world_version = *(int32_t *) world;
    if (world_version < 0 || world_version > MAX_SUPPORTED_MAP_VERSION)
        die("Unsupported world version: %d", world_version);

    int section_list_offset = 16;

    if (world_version >= 135) {
        if (memcmp("relogic", world + 4, 7) != 0)
            die_invalid("Bad magic");

        uint8_t file_type = world[11];
        if (file_type != 2)
            die("Not a world file (file type %u)", file_type);

        section_list_offset += 8;
    }

    section_count = *(int16_t *) (world + section_list_offset);
    if (section_count < 1)
        die_invalid("Invalid section list");

    section_offsets = (int32_t *) (world + section_list_offset + 2);
    extra = world + section_list_offset + 8;
}

void close_world(void) {
    munmap(world, world_file_size);
    close(world_fd);
}

int main(int argc, char *argv[]) {
    // allocate output message (width / scale * height / scale)
    // open world file
    // until past last block of capture region (or eof)
    //   read to top left of capture region
    //   encode block pixels to output message
    //   move to next block
    // close world file
    // encode output message
    // write output file

    cmd = argv[0];

    if (argc < 6 || argc > 7)
        die_usage();

    world_path = argv[1];

    int capture_left = atoi(argv[2]);
    int capture_top = atoi(argv[3]);
    int capture_width = atoi(argv[4]);
    int capture_height = atoi(argv[5]);

    int zoom = argc == 7 ? atoi(argv[6]) : 1;
    if (zoom < 1 || zoom > 5)
        die("Invalid zoom level (1 <= %d <= 5)", zoom);

    open_world();

    int32_t *header_offset = section_offsets;
    uint8_t *header = world + *header_offset;

    uint8_t *title_length = header;

    int32_t blocks_tall = *(int32_t *) (header + *title_length + 21);
    int32_t blocks_wide = *(int32_t *) (header + *title_length + 25);

    int max_x = blocks_wide / 2;
    int max_y = blocks_tall / 2;

    int max_width = max_x - capture_left;
    int max_height = max_y - capture_top;

    if (capture_left < -max_x || capture_left >= max_x)
        die("Invalid capture area (left = %d <= %d < %d)",
            -max_x, capture_left, max_x);

    if (capture_width < 1 || capture_width > max_width)
        die("Invalid capture area (width = %d < %d < %d)",
            0, capture_width, max_width);

    if (capture_top < -max_y || capture_top >= max_y)
        die("Invalid capture area (top = %d <= %d < %d)",
            -max_y, capture_top, max_y);

    if (capture_height < 1 || capture_height > max_height)
        die("Invalid capture area (height = %d < %d < %d)",
            0, capture_height, max_height);

    int scale = 1 << (5 - zoom);
    printf("Capturing %dx%d blocks (%d), %dx%d pixels (%d)\n",
           capture_width, capture_height, capture_width * capture_height,
           capture_width * scale, capture_height * scale,
           capture_width * capture_height * scale);

    uint8_t *tiles = world + section_offsets[1];
    uint8_t *tile = tiles;

    for (int x = -max_x; x < max_x; x++) {
        for (int y = -max_y; y < max_y; y++) {
            int capturing = x >= capture_left &&
                            x < capture_left + capture_width &&
                            y >= capture_top &&
                            y < capture_top + capture_height;

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

            if (capturing) {
                printf("%d, %d\n", x, y);
                printf("  Flags 1: %d\n", flags1);

                if (has_flags2)
                    printf("  Flags 2: %d\n", flags2);

                if (has_flags3)
                    printf("  Flags 3: %d\n", flags3);

                printf("  Active? %d\n", active);
            }

            if (active) {
                int type = *tile++;
                if (flags1 & 0x20)
                    type |= *tile++ << 8;

                if (capturing)
                    printf("  Type: %d\n", type);

                int16_t u = -1;
                int16_t v = -1;

                if (extra[type]) {
                    u = *(int16_t *) tile;
                    tile += 2;

                    v = *(int16_t *) tile;
                    tile += 2;
                }

                if (capturing && (u != 0 || v != 0))
                    printf("  U: %d, V: %d\n", u, v);

                if (flags3 & 0x8) {
                    uint8_t color = *tile++;

                    if (capturing)
                        printf("  Color: %d\n", color);
                }
            }

            if (flags1 & 4) {
                uint8_t wall = *tile++;

                if (capturing)
                    printf("  Wall: %d\n", wall);

                if (flags3 & 0x10) {
                    uint8_t wall_color = *tile++;

                    if (capturing)
                        printf("  Wall color: %d\n", wall_color);
                }
            }

            if (flags1 & 0x18) {
                int liquid = *tile++;

                if (capturing) {
                    printf("  Liquid: %d", liquid);

                    int liquid_type = flags1 & 0x18;
                    if (liquid_type == 0x10)
                        puts(" (Lava)");
                    else if (liquid_type == 0x18)
                        puts(" (Honey)");
                    else
                        printf(" (Unknown type: %d)\n", liquid_type);
                }
            }

            int rle = 0;
            int rle_format = flags1 >> 6;
            if (rle_format == 1) {
                rle = *tile++;
            } else if (rle_format == 2) {
                rle = *(int16_t *) tile;
                tile += 2;
            }

            if (rle > 0) {
                if (capturing)
                    printf("  RLE: %d\n", rle);

                y += rle;
                while (y >= max_y) {
                    x++;
                    y -= blocks_tall;
                }
            }
        }
    }

    close_world();

    return EXIT_SUCCESS;
}
