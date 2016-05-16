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
int32_t *world_version;

int16_t *section_count;
int32_t *section_offsets;

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

    world_version = (int32_t *) world;
    if (*world_version < 0 || *world_version > MAX_SUPPORTED_MAP_VERSION)
        die("Unsupported world version: %d", *world_version);

    int section_list_offset = 16;

    if (*world_version >= 135) {
        if (memcmp("relogic", world_version + 1, 7) != 0)
            die_invalid("Bad magic");

        uint8_t *file_type = world + 11;
        if (*file_type != 2)
            die("Not a world file (file type %u)", file_type);

        section_list_offset += 8;
    }

    section_count = (int16_t *) (world + section_list_offset);
    if (*section_count < 1)
        die_invalid("Invalid section list");

    section_offsets = (int32_t *) (section_count + 1);
}

void close_world(void) {
    munmap(world, world_file_size);
    close(world_fd);
}

int main(int argc, char *argv[]) {
    // capture region = {left, top, width, height}
    // scale = 2^(zoom-1)

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
        die("Invalid zoom level: %d", zoom);

    open_world();

    int32_t *header_offset = section_offsets;
    uint8_t *header = world + *header_offset;

    uint8_t *title_length = header;

    int32_t *blocks_tall = (int32_t *) (header + *title_length + 21);
    int32_t *blocks_wide = blocks_tall + 1;

    int max_x = *blocks_wide / 2;
    int max_y = *blocks_tall / 2;

    int max_width = max_x - capture_left;
    int max_height = max_y - capture_height;

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

    int scale = 2 ^ (5 - zoom);
    printf("Capturing %dx%d blocks (%d), %dx%d pixels (%d)\n",
           capture_width, capture_height, capture_width * capture_height,
           capture_width * scale, capture_height * scale, capture_width * capture_height * scale);

    close_world();

    return EXIT_SUCCESS;
}
