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

typedef enum {
    STATUS_OK,
    STATUS_WARNING,
    STATUS_ERROR
} PresetStatus;

int is_encoder_supported(const char *encoder_name);
int has_unknown_keys(const char *preset_file);
PresetStatus check_preset_health(const char *preset_filepath, BlurPreset *preset);
void show_critical_error_window(const char *error_title, const char *error_message);

#endif
