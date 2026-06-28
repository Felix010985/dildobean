#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adapt.h"
#include "queue.h"
#include "backend/ffmpeg.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("error: no input file provided.\n");
        printf("usage: %s <input_file>\n", argv[0]);
        getch();
        return 1;
    }
    char *input_filename = argv[1];

    char preset_filename[256] = {0};
    get_preset_from_priority_menu(preset_filename, sizeof(preset_filename));

    BlurPreset preset;
    load_and_validate_preset(preset_filename, &preset);

    char output_filename[1024];
    strncpy(output_filename, input_filename, sizeof(output_filename) - 1);
    output_filename[sizeof(output_filename) - 1] = '\0';

    char *dot = strrchr(output_filename, '.');
    if (dot) *dot = '\0';
    strncat(output_filename, "_blur.mp4", sizeof(output_filename) - strlen(output_filename) - 1);

    printf("target preset loaded: %s.\n", preset.name);
    printf("routing execution block.\n");

    if (strcmp(preset.backend, "dildoengine") == 0) {
        printf("launching dildoengine gpu backend.\n");
        char frames_str[16];
        snprintf(frames_str, sizeof(frames_str), "%d", preset.frames);

        _spawnlp(0, "dildoengine", "dildoengine",
                 input_filename, frames_str, preset.blur_mode, preset.output_fps, NULL);
    }
    else if (strcmp(preset.backend, "ffmpeg") == 0) {
        run_ffmpeg_backend(input_filename, output_filename, &preset);
    }
    else {
        printf("error: unknown backend configuration target.\n");
    }

    printf("session finished. press any key to exit.\n");
    getch();
    return 0;
}
