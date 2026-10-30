#include "display_hud_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    qw_hud_state_t state;
} preview_frame_t;

static const preview_frame_t FRAMES[] = {
    {"off", {.ble=QW_HUD_BLE_DISCONNECTED,.safety=QW_HUD_SAFETY_SAFE,.oe_disabled=true}},
    {"connecting", {.ble=QW_HUD_BLE_CONNECTING,.safety=QW_HUD_SAFETY_SAFE,.uptime_seconds=17,.oe_disabled=true}},
    {"connected", {.ble=QW_HUD_BLE_CONNECTED,.safety=QW_HUD_SAFETY_SAFE,.uptime_seconds=83,.oe_disabled=true,
                    .legs={{true,false,false},{false,true,false},
                           {true,true,true},{false,false,false}}}},
    {"failsafe", {.ble=QW_HUD_BLE_WEAK,.safety=QW_HUD_SAFETY_FAILSAFE,.uptime_seconds=3661,.oe_disabled=true}},
};

static unsigned expand_red(uint16_t pixel)
{
    return ((unsigned)(pixel >> 11) * 255U + 15U) / 31U;
}

static unsigned expand_green(uint16_t pixel)
{
    return ((unsigned)((pixel >> 5) & 0x3fU) * 255U + 31U) / 63U;
}

static unsigned expand_blue(uint16_t pixel)
{
    return ((unsigned)(pixel & 0x1fU) * 255U + 15U) / 31U;
}

static int write_ppm(const char *path, const uint16_t *fb,
                     uint16_t width, uint16_t height)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fprintf(file, "P6\n%u %u\n255\n", (unsigned)width,
                (unsigned)height) < 0) {
        fclose(file);
        return 0;
    }
    for (size_t i = 0; i < (size_t)width * height; i++) {
        unsigned char rgb[3] = {
            (unsigned char)expand_red(fb[i]),
            (unsigned char)expand_green(fb[i]),
            (unsigned char)expand_blue(fb[i]),
        };
        if (fwrite(rgb, sizeof(rgb), 1, file) != 1) {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

int main(int argc, char **argv)
{
    if (argc != 3 ||
        (strcmp(argv[1], "portrait") != 0 &&
         strcmp(argv[1], "landscape") != 0)) {
        fprintf(stderr, "usage: %s portrait|landscape OUTPUT_PREFIX\n", argv[0]);
        return 2;
    }

    const qw_hud_orientation_t orientation =
        strcmp(argv[1], "landscape") == 0
            ? QW_HUD_ORIENTATION_LANDSCAPE : QW_HUD_ORIENTATION_PORTRAIT;
    const uint16_t width = qw_hud_render_width(orientation);
    const uint16_t height = qw_hud_render_height(orientation);
    uint16_t *fb = malloc((size_t)width * height * sizeof(*fb));
    if (fb == NULL) {
        fprintf(stderr, "cannot allocate %ux%u framebuffer\n",
                (unsigned)width, (unsigned)height);
        return 1;
    }

    int result = 0;
    for (size_t i = 0; i < sizeof(FRAMES) / sizeof(FRAMES[0]); i++) {
        char path[512];
        int length = snprintf(path, sizeof(path), "%s-%02u-%s.ppm",
                              argv[2], (unsigned)i, FRAMES[i].name);
        if (length < 0 || (size_t)length >= sizeof(path) ||
            !qw_hud_render(fb, (size_t)width * height, &FRAMES[i].state,
                           orientation) ||
            !write_ppm(path, fb, width, height)) {
            fprintf(stderr, "failed to write %s frame %u\n", argv[1],
                    (unsigned)i);
            result = 1;
            break;
        }
        printf("%s\n", path);
    }
    free(fb);
    return result;
}
