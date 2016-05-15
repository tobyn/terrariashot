#include <errno.h>
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

typedef struct _world_handle {
    int fd;
    unsigned long bytes;
    uint8_t *start;
    uint8_t *cursor;
} world_handle;

typedef struct _world_error {
    char *message;
    size_t buffer_size;
} world_error;

char *cmd;

void usage() {
    fprintf(stderr,"Usage: %s <world> <left> <top> <width> <height> [zoom]\n",cmd);
    fputs("  world  - path to your .wld file\n",stderr);
    fputs("  left   - left edge coordinate of capture area\n",stderr);
    fputs("  top    - top edge coordinate of capture area\n",stderr);
    fputs("  width  - width of capture area in blocks\n",stderr);
    fputs("  height - height of capture area in blocks\n",stderr);
    fputs("  zoom   - zoom level [1-5]\n",stderr);
    exit(1);
}

void set_error(world_error *error, const char *format, ...) {
    va_list args;
    va_start(args,format);
    vsnprintf(error->message,error->buffer_size,format,args);
    va_end(args);
}

int open_world(const char *path, world_handle *world, world_error *error) {
    char *base_error_message;

    world->fd = open(path,O_RDONLY);
    if (world->fd == 1) {
        base_error_message = "Couldn't open world file";
        goto handle_error;
    }

    struct stat world_file_stats;
    if (fstat(world->fd,&world_file_stats) == -1) {
        base_error_message = "Couldn't read world file stats";
        goto handle_error;
    }

    world->bytes = (unsigned long) world_file_stats.st_size;
    world->start = mmap(NULL,world->bytes,PROT_READ,MAP_SHARED,world->fd,0);
    if (world->start == MAP_FAILED) {
        base_error_message = "Couldn't map world file";
        goto handle_error;
    }

    world->cursor = world->start;

    return 1;

    handle_error:
    if (error != NULL && error->buffer_size > 0)
        set_error(error,"%s: %s",base_error_message,strerror(errno));

    return 0;
}

int require_bytes(world_handle *world, unsigned long bytes, world_error *error) {
    unsigned long offset = world->cursor - world->start;
    if (offset + bytes >= world->bytes) {
        set_error(error, "Unexpected end of file");
        return 0;
    }

    //printf("Granting request for %lu bytes from offset %lu (%lu remaining)\n",
      //     bytes,offset,world->bytes-(offset+bytes));
    return 1;
}

int read_double(world_handle *world, double *out, world_error *error) {
    if (!require_bytes(world,8,error))
        return 0;

    *out = *(double*)world->cursor;
    world->cursor += 8;

    return 1;
}

int read_float(world_handle *world, float *out, world_error *error) {
    if (!require_bytes(world,4,error))
        return 0;

    *out = *(float*)world->cursor;
    world->cursor += 4;

    return 1;
}

int read_int16(world_handle *world, int16_t *out, world_error *error) {
    if (!require_bytes(world,2,error))
        return 0;

    *out = *(int32_t*)world->cursor;
    world->cursor += 2;

    return 1;
}

int read_int32(world_handle *world, int32_t *out, world_error *error) {
    if (!require_bytes(world,4,error))
        return 0;

    *out = *(int32_t*)world->cursor;
    world->cursor += 4;

    return 1;
}

int read_int32_array(world_handle *world, int32_t *out, size_t count, world_error *error) {
    for (; count > 0; count--) {
        if (!read_int32(world,out,error))
            return 0;

        out++;
    }

    return 1;
}

int read_int64(world_handle *world, int64_t *out, world_error *error) {
    if (!require_bytes(world,8,error))
        return 0;

    *out = *(int64_t*)world->cursor;
    world->cursor += 8;

    return 1;
}

int read_uint8(world_handle *world, uint8_t *out, world_error *error) {
    if (!require_bytes(world,1,error))
        return 0;

    *out = *world->cursor++;

    return 1;
}

int read_uint8_array(world_handle *world, uint8_t *out, size_t count, world_error *error) {
    for (; count > 0; count--) {
        if (!read_uint8(world,out,error))
            return 0;

        out++;
    }

    return 1;
}

int read_string(world_handle *world, char *out, world_error *error) {
    uint8_t length;
    if (!read_uint8(world,&length,error))
        return 0;

    if (!require_bytes(world,length,error))
        return 0;

    memcpy(out,world->cursor,length);
    out[length] = '\0';

    world->cursor += length;

    return 1;
}

int seek_forward(world_handle *world, unsigned long bytes, world_error *error) {
    if (!require_bytes(world,bytes,error))
        return 0;

    world->cursor += bytes;

    return 1;
}

int seek_to(world_handle *world, unsigned long offset, world_error *error) {
    world->cursor = world->start;

    if (!require_bytes(world,offset,error))
        return 0;

    world->cursor += offset;

    return 1;
}

void close_world(world_handle *world) {
    munmap(world->start,world->bytes);
    close(world->fd);
}

int main(int argc, char *argv[]) {
    // read from command line: world path, top, left, width, height, [zoom=1]
    //  world_path = string
    //  left = int, left edge of the capture region [-WORLD_WIDTH/2, WORLD_WIDTH/2)
    //  top = int, top edge of the capture region [-WORLD_HEIGHT/2, WORLD_HEIGHT/2)
    //  width = width of the capture region in blocks [1, WORLD_WIDTH/2-left]
    //  height = height of the capture region in blocks [1, WORLD_HEIGHT/2-top]
    //  zoom = int, the zoom level [1-5] (1 image pixel represents scale game pixels square)

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

    // critical question: HOW FAST CAN WE SEEK AND LOAD OUR CAPTURE REGION?
    // it needs to be a reasonable amount of time to happen in a web request

    cmd = argv[0];

    if (argc < 6 || argc > 7)
        usage();

    char *world_path = argv[1];

    /*
    int capture_left = atoi(argv[2]);
    int capture_top = atoi(argv[3]);
    int capture_width = atoi(argv[4]);
    int capture_height = atoi(argv[5]);
     */

    int zoom = 1;
    if (argc == 7)
        zoom = atoi(argv[6]);

    if (zoom < 1 || zoom > 5) {
        fprintf(stderr,"Invalid zoom level: %d\n",zoom);
        exit(1);
    }

    world_error error;
    char error_message[4096];
    error.message = error_message;
    error.buffer_size = 4096;

    world_handle world;

    if (!open_world(world_path,&world,&error))
        goto handle_early_error;

    int32_t map_version;
    if (!read_int32(&world,&map_version,&error))
        goto handle_opened_error;

    printf("Map version: %d\n",map_version);
    if (map_version < 0 || map_version > MAX_SUPPORTED_MAP_VERSION) {
        set_error(&error, "Unsupported map version: %d", map_version);
        goto handle_opened_error;
    }


    if (map_version >= 135) {
        if (memcmp("relogic",world.cursor,7) != 0) {
            set_error(&error, "Invalid map file.");
            goto handle_opened_error;
        }

        if (!seek_forward(&world,7,&error))
            goto handle_opened_error;

        uint8_t file_type;
        if (!read_uint8(&world,&file_type,&error))
            goto handle_opened_error;

        if (file_type != 2) {
            set_error(&error,"Not a map file. File type: %u.\n",file_type);
            goto handle_opened_error;
        }
    }

    if (!seek_forward(&world,12,&error))
        goto handle_opened_error;

    int16_t section_count;
    if (!read_int16(&world,&section_count,&error))
        goto handle_opened_error;
    printf("Number of sections: %d.\n",section_count);

    int32_t *sections = malloc(section_count * sizeof(int32_t));
    if (sections == NULL) {
        set_error(&error,"Couldn't allocate section buffer");
        goto handle_opened_error;
    }

    for (int i = 0; i < section_count; i++) {
        if (!read_int32(&world,sections+i,&error))
            goto handle_sections_error;
    }

    int16_t tile_count;
    if (!read_int16(&world,&tile_count,&error))
        goto handle_sections_error;
    printf("Tiles: %d\n",tile_count);

    uint8_t *extra = malloc(tile_count * sizeof(uint8_t));
    if (extra == NULL) {
        set_error(&error,"Couldn't allocate extra buffer");
        goto handle_sections_error;
    }

    uint8_t mask = 0x80;
    uint8_t bits = 0;
    for (int i = 0; i < tile_count; i++) {
        if (mask == 0x80) {
            if (!read_uint8(&world,&bits,&error))
                goto handle_extra_error;

            mask = 1;
        } else {
            mask <<= 1;
        }
        extra[i] = bits;
    }

    if (!seek_to(&world,(unsigned long)sections[0],&error))
        goto handle_extra_error;

    char title[256];
    if (!read_string(&world,title,&error))
        goto handle_extra_error;
    printf("Title: %s\n",title);

    int32_t world_id;
    if (!read_int32(&world,&world_id,&error))
        goto handle_extra_error;
    printf("World ID: %d\n",world_id);

    int32_t world_left, world_right, world_top, world_bottom;
    if (!read_int32(&world,&world_left,&error) ||
            !read_int32(&world,&world_right,&error) ||
            !read_int32(&world,&world_top,&error) ||
            !read_int32(&world,&world_bottom,&error))
        goto handle_extra_error;
    printf("World left: %d; right: %d; top: %d; bottom: %d\n",
        world_left, world_right, world_top, world_bottom);

    int32_t tiles_high, tiles_wide;
    if (!read_int32(&world,&tiles_high,&error) ||
        !read_int32(&world,&tiles_wide,&error))
        goto handle_extra_error;
    printf("Tiles: %dx%d\n",tiles_wide,tiles_high);

    uint8_t expert_mode = 0;
    if (map_version >= 112) {
        if (!read_uint8(&world,&expert_mode,&error))
            goto handle_extra_error;

        if (expert_mode)
            puts("Expert mode!");
    }

    int64_t creation_time = 0;
    if (map_version >= 141) {
        if (!read_int64(&world,&creation_time,&error))
            goto handle_extra_error;

        printf("Creation timestamp: %ld\n",creation_time);
    }

    uint8_t moon_type;
    if (!read_uint8(&world,&moon_type,&error))
        goto handle_extra_error;
    printf("Moon type: %u\n",moon_type);

    int32_t tree_x[3];
    if (!read_int32_array(&world,tree_x,3,&error))
        goto handle_extra_error;
    printf("Tree X: %d, %d, %d\n",tree_x[0],tree_x[1],tree_x[2]);

    int32_t tree_style[4];
    if (!read_int32_array(&world,tree_style,4,&error))
        goto handle_extra_error;
    printf("Tree style: %d, %d, %d, %d\n",
           tree_style[0],tree_style[1],tree_style[2],tree_style[3]);

    int32_t cave_back_x[3];
    if (!read_int32_array(&world,cave_back_x,3,&error))
        goto handle_extra_error;
    printf("Cave back X: %d, %d, %d\n",
           cave_back_x[0],cave_back_x[1],cave_back_x[2]);

    int32_t cave_back_style[4];
    if (!read_int32_array(&world,cave_back_style,4,&error))
        goto handle_extra_error;
    printf("Cave back style: %d, %d, %d, %d\n",
           cave_back_style[0],cave_back_style[1],
           cave_back_style[2],cave_back_style[3]);

    int32_t ice_back_style;
    if (!read_int32(&world,&ice_back_style,&error))
        goto handle_extra_error;
    printf("Ice back style: %d\n",ice_back_style);

    int32_t jungle_back_style;
    if (!read_int32(&world,&jungle_back_style,&error))
        goto handle_extra_error;
    printf("Jungle back style: %d\n",jungle_back_style);

    int32_t hell_back_style;
    if (!read_int32(&world,&hell_back_style,&error))
        goto handle_extra_error;
    printf("Hell back style: %d\n",hell_back_style);

    int32_t spawn_x, spawn_y;
    if (!read_int32(&world,&spawn_x,&error) ||
            !read_int32(&world,&spawn_y,&error))
        goto handle_extra_error;
    printf("Spawn: %d, %d\n",spawn_x, spawn_y);

    double ground_level;
    if (!read_double(&world,&ground_level,&error))
        goto handle_extra_error;
    printf("Ground level: %f\n",ground_level);

    double rock_level;
    if (!read_double(&world,&rock_level,&error))
        goto handle_extra_error;
    printf("Rock level: %f\n",rock_level);

    double game_time;
    if (!read_double(&world,&game_time,&error))
        goto handle_extra_error;
    printf("Game time: %f\n",game_time);

    uint8_t day;
    if (!read_uint8(&world,&day,&error))
        goto handle_extra_error;
    printf("Day? %d\n",day);

    int32_t moon_phase;
    if (!read_int32(&world,&moon_phase,&error))
        goto handle_extra_error;
    printf("Moon phase: %d\n",moon_phase);

    uint8_t blood_moon;
    if (!read_uint8(&world,&blood_moon,&error))
        goto handle_extra_error;
    printf("Blood moon? %d\n",blood_moon);

    uint8_t eclipse;
    if (!read_uint8(&world,&eclipse,&error))
        goto handle_extra_error;
    printf("Eclipse? %d\n",eclipse);

    int32_t dungeon_x, dungeon_y;
    if (!read_int32(&world,&dungeon_x,&error) ||
            !read_int32(&world,&dungeon_y,&error))
        goto handle_extra_error;
    printf("Dungeon: %d, %d\n",dungeon_x, dungeon_y);

    uint8_t crimson;
    if (!read_uint8(&world,&crimson,&error))
        goto handle_extra_error;
    printf("Crimson? %d\n",crimson);

    uint8_t killed_boss_1;
    if (!read_uint8(&world,&killed_boss_1,&error))
        goto handle_extra_error;
    printf("Killed boss 1? %d\n",killed_boss_1);

    uint8_t killed_boss_2;
    if (!read_uint8(&world,&killed_boss_2,&error))
        goto handle_extra_error;
    printf("Killed boss 2? %d\n",killed_boss_2);

    uint8_t killed_boss_3;
    if (!read_uint8(&world,&killed_boss_3,&error))
        goto handle_extra_error;
    printf("Killed boss 3? %d\n",killed_boss_3);

    uint8_t killed_queen_bee;
    if (!read_uint8(&world,&killed_queen_bee,&error))
        goto handle_extra_error;
    printf("Killed Queen Bee? %d\n",killed_queen_bee);

    uint8_t killed_mech_boss_1;
    if (!read_uint8(&world,&killed_mech_boss_1,&error))
        goto handle_extra_error;
    printf("Killed mech boss 1? %d\n",killed_mech_boss_1);

    uint8_t killed_mech_boss_2;
    if (!read_uint8(&world,&killed_mech_boss_2,&error))
        goto handle_extra_error;
    printf("Killed mech boss 2? %d\n",killed_mech_boss_2);

    uint8_t killed_mech_boss_3;
    if (!read_uint8(&world,&killed_mech_boss_3,&error))
        goto handle_extra_error;
    printf("Killed mech boss 3? %d\n",killed_mech_boss_3);

    uint8_t killed_any_mech_boss;
    if (!read_uint8(&world,&killed_any_mech_boss,&error))
        goto handle_extra_error;
    printf("Killed any mech bosses? %d\n",killed_any_mech_boss);

    uint8_t killed_plantera;
    if (!read_uint8(&world,&killed_plantera,&error))
        goto handle_extra_error;
    printf("Killed Plantera? %d\n",killed_plantera);

    uint8_t killed_golem;
    if (!read_uint8(&world,&killed_golem,&error))
        goto handle_extra_error;
    printf("Killed Golem? %d\n",killed_golem);

    uint8_t killed_king_slime = 0;
    if (map_version >= 118) {
        if (!read_uint8(&world,&killed_king_slime,&error))
            goto handle_extra_error;
    }
    printf("Killed King Slime? %d\n",killed_king_slime);

    uint8_t saved_tinkerer;
    if (!read_uint8(&world,&saved_tinkerer,&error))
        goto handle_extra_error;
    printf("Saved Goblin Tinkerer? %d\n",saved_tinkerer);

    uint8_t saved_wizard;
    if (!read_uint8(&world,&saved_wizard,&error))
        goto handle_extra_error;
    printf("Saved Wizard? %d\n",saved_wizard);

    uint8_t saved_mechanic;
    if (!read_uint8(&world,&saved_mechanic,&error))
        goto handle_extra_error;
    printf("Saved Mechanic? %d\n",saved_mechanic);

    uint8_t killed_goblin_army;
    if (!read_uint8(&world,&killed_goblin_army,&error))
        goto handle_extra_error;
    printf("Killed goblin army? %d\n",killed_goblin_army);

    uint8_t killed_clowns;
    if (!read_uint8(&world,&killed_clowns,&error))
        goto handle_extra_error;
    printf("Killed clowns? %d\n",killed_clowns);

    uint8_t killed_frost_legion;
    if (!read_uint8(&world,&killed_frost_legion,&error))
        goto handle_extra_error;
    printf("Killed frost legion? %d\n",killed_frost_legion);

    uint8_t killed_pirates;
    if (!read_uint8(&world,&killed_pirates,&error))
        goto handle_extra_error;
    printf("Killed pirates? %d\n",killed_pirates);

    uint8_t smashed_orb;
    if (!read_uint8(&world,&smashed_orb,&error))
        goto handle_extra_error;
    printf("Smashed an orb? %d\n",smashed_orb);

    uint8_t meteor_spawned;
    if (!read_uint8(&world,&meteor_spawned,&error))
        goto handle_extra_error;
    printf("Spawned a meteor? %d\n",meteor_spawned);

    uint8_t shadow_orbs;
    if (!read_uint8(&world,&shadow_orbs,&error))
        goto handle_extra_error;
    printf("Shadow orbs: %d\n",shadow_orbs);

    int32_t altars_smashed;
    if (!read_int32(&world,&altars_smashed,&error))
        goto handle_extra_error;
    printf("Altars smashed: %d\n",altars_smashed);

    uint8_t hard_mode;
    if (!read_uint8(&world,&hard_mode,&error))
        goto handle_extra_error;
    printf("Hard mode? %d\n",hard_mode);

    int32_t invasion_delay;
    if (!read_int32(&world,&invasion_delay,&error))
        goto handle_extra_error;
    printf("Invasion delay: %d\n",invasion_delay);

    int32_t invasion_size;
    if (!read_int32(&world,&invasion_size,&error))
        goto handle_extra_error;
    printf("Invasion size: %d\n",invasion_size);

    int32_t invasion_type;
    if (!read_int32(&world,&invasion_type,&error))
        goto handle_extra_error;
    printf("Invasion type: %d\n",invasion_type);

    double invasion_x;
    if (!read_double(&world,&invasion_x,&error))
        goto handle_extra_error;
    printf("Invasion X: %f\n",invasion_x);

    double slime_rain_time = 0;
    if (map_version >= 118) {
        if (!read_double(&world,&slime_rain_time,&error))
            goto handle_extra_error;
    }
    printf("Slime rain time: %f\n",slime_rain_time);

    uint8_t sundial_cooldown = 0;
    if (map_version >= 113) {
        if (!read_uint8(&world,&sundial_cooldown,&error))
            goto handle_extra_error;
    }
    printf("Sundial cooldown: %d\n",sundial_cooldown);

    uint8_t raining;
    if (!read_uint8(&world,&raining,&error))
        goto handle_extra_error;
    printf("Raining? %d\n",raining);

    int32_t rain_time;
    if (!read_int32(&world,&rain_time,&error))
        goto handle_extra_error;
    printf("Rain time: %d\n",rain_time);

    float max_rain;
    if (!read_float(&world,&max_rain,&error))
        goto handle_extra_error;
    printf("Max rain: %f\n",max_rain);

    int32_t ore_tier_1;
    if (!read_int32(&world,&ore_tier_1,&error))
        goto handle_extra_error;
    printf("Ore tier 1: %d\n",ore_tier_1);

    int32_t ore_tier_2;
    if (!read_int32(&world,&ore_tier_2,&error))
        goto handle_extra_error;
    printf("Ore tier 2: %d\n",ore_tier_2);

    int32_t ore_tier_3;
    if (!read_int32(&world,&ore_tier_3,&error))
        goto handle_extra_error;
    printf("Ore tier 3: %d\n",ore_tier_3);

    uint8_t styles[8];
    if (!read_uint8_array(&world,styles,8,&error))
        goto handle_extra_error;
    printf("Styles: %d, %d, %d, %d, %d, %d, %d, %d\n",
           styles[0],styles[1],styles[2],styles[3],
           styles[4],styles[5],styles[6],styles[7]);

    int32_t clouds;
    if (!read_int32(&world,&clouds,&error))
        goto handle_extra_error;
    printf("Clouds: %d\n",clouds);

    int16_t num_clouds;
    if (!read_int16(&world,&num_clouds,&error))
        goto handle_extra_error;
    printf("Number of clouds: %d\n",num_clouds);

    float wind_speed;
    if (!read_float(&world,&wind_speed,&error))
        goto handle_extra_error;
    printf("Wind speed: %f\n",wind_speed);

    int32_t num_anglers = 0;
    if (map_version >= 95) {
        if (!read_int32(&world,&num_anglers,&error))
            goto handle_extra_error;
    }
    printf("Number of anglers: %d\n",num_anglers);

    if (map_version >= 95) {
        char angler[256];
        for (int i = 0; i < num_anglers; i++) {
            if (!read_string(&world, angler, &error))
                goto handle_extra_error;

            printf("Found an angler: %s\n", angler);
        }
    }

    uint8_t saved_angler = 0;
    if (map_version >= 99) {
        if (!read_uint8(&world,&saved_angler,&error))
            goto handle_extra_error;
    }
    printf("Saved the angler? %d\n",saved_angler);

    int32_t angler_quest = 0;
    if (map_version >= 101) {
        if (!read_int32(&world,&angler_quest,&error))
            goto handle_extra_error;
    }
    printf("Angler quest: %d\n",angler_quest);

    uint8_t saved_stylist = 0;
    if (map_version >= 104) {
        if (!read_uint8(&world,&saved_stylist,&error))
            goto handle_extra_error;
    }
    printf("Saved Stylist? %d\n",saved_stylist);

    uint8_t saved_tax_collector = 0;
    if (map_version >= 104) {
        if (!read_uint8(&world,&saved_tax_collector,&error))
            goto handle_extra_error;
    }
    printf("Saved Tax Collector? %d\n",saved_tax_collector);

    int32_t invasion_size_start = 0;
    if (map_version >= 107) {
        if (!read_int32(&world,&invasion_size_start,&error))
            goto handle_extra_error;
    }
    printf("Invasion size start: %d\n",invasion_size_start);

    int32_t cultist_delay = 0;
    if (map_version >= 108) {
        if (!read_int32(&world,&cultist_delay,&error))
            goto handle_extra_error;
    }
    printf("Cultist delay: %d\n",cultist_delay);

    int16_t num_killed = 0;
    if (map_version >= 109) {
        if (!read_int16(&world,&num_killed,&error))
            goto handle_extra_error;

        int32_t kill_count;
        for (int i = 0; i < num_killed; i++) {
            if (!read_int32(&world,&kill_count,&error))
                goto handle_extra_error;

            //printf("Kill count %d: %d\n",i,kill_count);
        }
    }
    printf("Kill count records: %d\n",num_killed);

    uint8_t fast_forward_time = 0;
    if (map_version >= 128) {
        if (!read_uint8(&world,&fast_forward_time,&error))
            goto handle_extra_error;
    }
    printf("Fast forward time: %d\n",fast_forward_time);

    uint8_t killed_fishron = 0;
    uint8_t killed_martians = 0;
    uint8_t killed_ancient_cultist = 0;
    uint8_t killed_moon_lord = 0;
    uint8_t killed_pumpking = 0;
    uint8_t killed_mourning_wood = 0;
    uint8_t killed_ice_queen = 0;
    uint8_t killed_santa = 0;
    uint8_t killed_everscream = 0;

    if (map_version >= 131) {
        if (!read_uint8(&world,&killed_fishron,&error) ||
                !read_uint8(&world,&killed_martians,&error) ||
                !read_uint8(&world,&killed_ancient_cultist,&error) ||
                !read_uint8(&world,&killed_moon_lord,&error) ||
                !read_uint8(&world,&killed_pumpking,&error) ||
                !read_uint8(&world,&killed_mourning_wood,&error) ||
                !read_uint8(&world,&killed_ice_queen,&error) ||
                !read_uint8(&world,&killed_santa,&error) ||
                !read_uint8(&world,&killed_everscream,&error))
            goto handle_extra_error;
    }

    printf("Killed Duke Fishron? %d\n",killed_fishron);
    printf("Killed Martians? %d\n",killed_martians);
    printf("Killed Ancient Cultist? %d\n",killed_ancient_cultist);
    printf("Killed Moon Lord? %d\n",killed_moon_lord);
    printf("Killed Pumpking? %d\n",killed_pumpking);
    printf("Killed Mourning Wood? %d\n",killed_mourning_wood);
    printf("Killed Ice Queen? %d\n",killed_ice_queen);
    printf("Killed Santa NK-1? %d\n",killed_santa);
    printf("Killed Everscream? %d\n",killed_everscream);

    uint8_t killed_solar = 0;
    uint8_t killed_vortex = 0;
    uint8_t killed_nebula = 0;
    uint8_t killed_stardust = 0;
    uint8_t active_solar = 0;
    uint8_t active_vortex = 0;
    uint8_t active_nebula = 0;
    uint8_t active_stardust = 0;
    uint8_t lunar_apocalypse = 0;

    if (map_version >= 140) {
        if (!read_uint8(&world,&killed_solar,&error) ||
            !read_uint8(&world,&killed_vortex,&error) ||
            !read_uint8(&world,&killed_nebula,&error) ||
            !read_uint8(&world,&killed_stardust,&error) ||
            !read_uint8(&world,&active_solar,&error) ||
            !read_uint8(&world,&active_vortex,&error) ||
            !read_uint8(&world,&active_nebula,&error) ||
            !read_uint8(&world,&active_stardust,&error) ||
            !read_uint8(&world,&lunar_apocalypse,&error))
            goto handle_extra_error;
    }

    printf("Solar pillar: Killed? %d Active? %d\n",killed_solar,active_solar);
    printf("Vortex pillar: Killed? %d Active? %d\n",killed_vortex,active_vortex);
    printf("Nebula pillar: Killed? %d Active? %d\n",killed_nebula,active_nebula);
    printf("Stardust pillar: Killed? %d Active? %d\n",killed_stardust,active_stardust);
    printf("Lunar apocalypse? %d\n",lunar_apocalypse);

    close_world(&world);
    return EXIT_SUCCESS;

    handle_extra_error:
    free(extra);

    handle_sections_error:
    free(sections);

    handle_opened_error:
    close_world(&world);

    handle_early_error:
    fprintf(stderr,"%s\n",error.message);
    return EXIT_FAILURE;
}
