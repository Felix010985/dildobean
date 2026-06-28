#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct {
    char name[64];
    char backend[32];
    int frames;
    char blur_mode[16];
    char output_fps[8];
    char encoder[32];
    char bitrate[64];
    int quality;
} BlurPreset;


void get_preset_from_priority_menu(char *out_filename, size_t max_len);

void load_and_validate_preset(const char *preset_file, BlurPreset *preset);

void apply_preset_fallbacks(const char *preset_file, BlurPreset *preset);

#endif
