#include "queue.h"
#include "adapt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max_presets 32

#define clr_reset   "\033[0m"
#define clr_bold    "\033[1m"
#define clr_cyan    "\033[36m"
#define clr_magenta "\033[35m"
#define clr_green   "\033[32m"
#define clr_yellow  "\033[33m"
#define clr_red     "\033[31m"
#define clr_gray    "\033[90m"

typedef enum { TYPE_STR, TYPE_INT } ParamType;

typedef struct {
    const char *key;
    ParamType type;
    size_t offset;
    size_t size;
    const char *def_str;
    int def_int;
    int min_val;
    int max_val;
} IniFieldMeta;

static const IniFieldMeta preset_meta[] = {
    {"name",       TYPE_STR, offsetof(BlurPreset, name),       sizeof(((BlurPreset*)0)->name),       "default", 0, 0, 0},
    {"backend",    TYPE_STR, offsetof(BlurPreset, backend),    sizeof(((BlurPreset*)0)->backend),    "ffmpeg",                0, 0, 0},
    {"blur_mode",  TYPE_STR, offsetof(BlurPreset, blur_mode),   sizeof(((BlurPreset*)0)->blur_mode),   "gaussian",              0, 0, 0},
    {"output_fps", TYPE_STR, offsetof(BlurPreset, output_fps),  sizeof(((BlurPreset*)0)->output_fps),  "60",                    0, 0, 0},
    {"encoder",    TYPE_STR, offsetof(BlurPreset, encoder),     sizeof(((BlurPreset*)0)->encoder),     "libx264",               0, 0, 0},
    {"bitrate",    TYPE_STR, offsetof(BlurPreset, bitrate),     sizeof(((BlurPreset*)0)->bitrate),     "-crf 14",               0, 0, 0},
    {"frames",     TYPE_INT, offsetof(BlurPreset, frames),     0,                                    NULL,                    7, 1, 512},
    {"quality",    TYPE_INT, offsetof(BlurPreset, quality),    0,                                    NULL,                    14, 0, 51}
};
#define META_COUNT (sizeof(preset_meta) / sizeof(preset_meta[0]))


static void trim_whitespace(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[len - 1] = '\0';
        len--;
    }
}

static void clear_screen(void) {
    printf("\033[H\033[J");
}

int validate_preset_structure(BlurPreset *preset) {
    int was_corrupted = 0;

    for (size_t i = 0; i < META_COUNT; i++) {
        const IniFieldMeta *meta = &preset_meta[i];

        if (meta->type == TYPE_STR) {
            char *field_ptr = (char*)((char*)preset + meta->offset);
            if (strlen(field_ptr) == 0) {
                printf("warning: missing '%s', using fallback '%s'\n", meta->key, meta->def_str);
                snprintf(field_ptr, meta->size, "%s", meta->def_str);
                was_corrupted = 1;
            }
        } else if (meta->type == TYPE_INT) {
            int *field_ptr = (int*)((char*)preset + meta->offset);
            if (*field_ptr < meta->min_val || *field_ptr > meta->max_val) {
                printf("warning: invalid range for '%s', using fallback %d\n", meta->key, meta->def_int);
                *field_ptr = meta->def_int;
                was_corrupted = 1;
            }
        }
    }
    return was_corrupted;
}

void patch_corrupted_lines(const char *preset_file, BlurPreset *preset) {
    FILE *fr = fopen(preset_file, "r");
    if (!fr) return;

    char *file_buffer = calloc(1, 16384);
    if (!file_buffer) {
        fclose(fr);
        return;
    }

    char line[256];
    int found_flags[META_COUNT] = {0};

    while (fgets(line, sizeof(line), fr)) {
        char *start = line;
        while (*start == ' ' || *start == '\t') start++;

        if (start[0] == '#' || start[0] == ';' || start[0] == '\n' || start[0] == '\r' || start[0] == '[') {
            strncat(file_buffer, line, 16384 - strlen(file_buffer) - 1);
            continue;
        }

        char *eq = strchr(start, '=');
        if (!eq) {
            continue;
        }

        int line_patched = 0;
        for (size_t i = 0; i < META_COUNT; i++) {
            const IniFieldMeta *meta = &preset_meta[i];
            size_t key_len = strlen(meta->key);

            if (strncmp(start, meta->key, key_len) == 0 && start[key_len] == '=') {
                char patch_buf[256];
                int spaces = start - line;

                int offset = 0;
                for (int s = 0; s < spaces; s++) {
                    patch_buf[offset++] = line[s];
                }

                if (meta->type == TYPE_STR) {
                    snprintf(patch_buf + offset, sizeof(patch_buf) - offset, "%s=%s\n", meta->key, (char*)((char*)preset + meta->offset));
                } else {
                    snprintf(patch_buf + offset, sizeof(patch_buf) - offset, "%s=%d\n", meta->key, *(int*)((char*)preset + meta->offset));
                }

                strncat(file_buffer, patch_buf, 16384 - strlen(file_buffer) - 1);
                found_flags[i] = 1;
                line_patched = 1;
                break;
            }
        }

        if (!line_patched) {
            strncat(file_buffer, line, 16384 - strlen(file_buffer) - 1);
        }
    }
    fclose(fr);

    for (size_t i = 0; i < META_COUNT; i++) {
        if (!found_flags[i]) {
            const IniFieldMeta *meta = &preset_meta[i];
            char append_buf[256];
            if (meta->type == TYPE_STR) {
                snprintf(append_buf, sizeof(append_buf), "%s=%s\n", meta->key, (char*)((char*)preset + meta->offset));
            } else {
                snprintf(append_buf, sizeof(append_buf), "%s=%d\n", meta->key, *(int*)((char*)preset + meta->offset));
            }
            strncat(file_buffer, append_buf, 16384 - strlen(file_buffer) - 1);
        }
    }

    FILE *fw = fopen(preset_file, "w");
    if (fw) {
        fputs(file_buffer, fw);
        fclose(fw);
    }

    free(file_buffer);
}

// static int has_unknown_keys(const char *preset_file) {
//     FILE *f = fopen(preset_file, "r");
//     if (!f) return 0;
//
//     char line[256];
//     int found_unknown = 0;
//
//     while (fgets(line, sizeof(line), f)) {
//         char *start = line;
//         while (*start == ' ' || *start == '\t') start++;
//
//         if (*start == '#' || *start == ';' || *start == '\n' || *start == '\r' || *start == '[') {
//             continue;
//         }
//
//         char *eq = strchr(start, '=');
//         if (!eq) {
//             found_unknown = 1;
//             break;
//         }
//
//         size_t key_len = eq - start;
//
//         int is_valid_key = 0;
//         for (size_t i = 0; i < META_COUNT; i++) {
//             if (strncmp(start, preset_meta[i].key, key_len) == 0 && strlen(preset_meta[i].key) == key_len) {
//                 is_valid_key = 1;
//                 break;
//             }
//         }
//
//         if (!is_valid_key) {
//             found_unknown = 1;
//             break;
//         }
//     }
//
//     fclose(f);
//     return found_unknown;
// }

int is_encoder_supported(const char *encoder_name) {
    if (strlen(encoder_name) == 0) return 0;
    FILE *fp = popen("ffmpeg -encoders -loglevel quiet", "r");
    if (!fp) return 0;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, encoder_name) != NULL) {
            found = 1;
            break;
        }
    }
    pclose(fp);
    return found;
}

int has_unknown_keys(const char *preset_file) {
    FILE *f = fopen(preset_file, "r");
    if (!f) return 0;

    char line[256];
    int found_unknown = 0;

    while (fgets(line, sizeof(line), f)) {
        char *start = line;
        while (*start == ' ' || *start == '\t') start++;

        if (*start == '#' || *start == ';' || *start == '\n' || *start == '\r' || *start == '[') {
            continue;
        }

        char *eq = strchr(start, '=');
        if (!eq) {
            found_unknown = 1;
            break;
        }

        size_t key_len = eq - start;
        int is_valid_key = 0;
        for (size_t i = 0; i < META_COUNT; i++) {
            if (strncmp(start, preset_meta[i].key, key_len) == 0 && strlen(preset_meta[i].key) == key_len) {
                is_valid_key = 1;
                break;
            }
        }

        if (!is_valid_key) {
            found_unknown = 1;
            break;
        }
    }
    fclose(f);
    return found_unknown;
}
PresetStatus check_preset_health(const char *preset_filepath, BlurPreset *preset) {
    if (strcmp(preset->backend, "ffmpeg") == 0) {
        if (!is_encoder_supported(preset->encoder)) return STATUS_ERROR;
    }
    if (strlen(preset->backend) == 0 || (strcmp(preset->backend, "ffmpeg") != 0 && strcmp(preset->backend, "dildoengine") != 0)) {
        return STATUS_ERROR;
    }

    if (has_unknown_keys(preset_filepath)) {
        return STATUS_WARNING;
    }

    int fps_val = atoi(preset->output_fps);
    if (fps_val < 24 || fps_val > 240 || preset->frames < 2 || preset->frames > 32) {
        return STATUS_WARNING;
    }

    return STATUS_OK;
}

void show_critical_error_window(const char *error_title, const char *error_message) {
    clear_screen();
    printf("%s|--------------------------------------------------------|%s\n", clr_red, clr_reset);
    printf("%s| %-54s |%s\n", clr_red, error_title, clr_reset);
    printf("%s|--------------------------------------------------------|%s\n\n", clr_red, clr_reset);
    printf("  %sDetail:%s %s\n\n", clr_bold, clr_reset, error_message);
    printf("%s  [ Press any key to return to profile selection ]%s\n", clr_gray, clr_reset);
    getch();
}

void show_preset_code_preview(const char *preset_name, const char *preset_filepath) {
    clear_screen();
    printf("display name: %s\n", preset_name);
    printf("path: %s\n", preset_filepath);
    printf("--------------------------------------------------\n");

    FILE *f = fopen(preset_filepath, "r");
    if (!f) {
        printf("error: cannot open this preset file.\n");
    } else {
        char file_line[256];
        while (fgets(file_line, sizeof(file_line), f)) {
            printf("    %s", file_line);
        }
        fclose(f);
    }

    printf("--------------------------------------------------\n");
    printf("\033[90m[\033[0m\033[1;36m ^b \033[0m\033[90mexit code preview ]          [\033[0m\033[1;36m ^m \033[0m\033[90mmodify in gnu/nano ]\033[0m\n");

    while (1) {
        char action = getch();
        if (action == 2) {
            break;
        }
        if (action == 13) { // ^m (ctrl+m / enter) - spawn system editor.
            _spawnlp(0, "nano", "nano", preset_filepath, NULL);
            // recursively reload state to show new modified modifications.
            show_preset_code_preview(preset_name, preset_filepath);
            break;
        }
    }
}

void get_preset_from_priority_menu(char *out_filename, size_t max_len) {
    FILE *pf = fopen("priority.ini", "r");
    if (!pf) {
        printf("priority.ini not found. creating default template.\n");
        pf = fopen("priority.ini", "w");
        if (pf) {
            fprintf(pf, "# make up to 32 presets for every scenario\n");
            fprintf(pf, "[profiles]\n");
            fprintf(pf, "240fps -> 30fps skywars = c:/dildobean/presets/240to30.ini\n");
            fprintf(pf, "480fps -> 60fps nodebuff = presets/480to60.ini\n");
            fclose(pf);
        }
        pf = fopen("priority.ini", "r");
        if (!pf) {
            printf("error: cannot read priority.ini.\n");
            exit(1);
        }
    }

    // allocate buffers to separate display names from paths.
    char **names = malloc(max_presets * sizeof(char *));
    char **paths = malloc(max_presets * sizeof(char *));
    int count = 0;
    char line[512];

    clear_screen();
    printf("--- available rendering presets ---\n");

    while (fgets(line, sizeof(line), pf) && count < max_presets) {
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        // trim whitespace for both sides.
        while (*key == ' ' || *key == '\t') key++;
        trim_whitespace(key);
        while (*val == ' ' || *val == '\t') val++;
        trim_whitespace(val);

        if (strlen(key) > 0 && strlen(val) > 0) {
            names[count] = malloc(strlen(key) + 1);
            paths[count] = malloc(strlen(val) + 1);
            strcpy(names[count], key);
            strcpy(paths[count], val);

            BlurPreset test_preset = {0};
            load_and_validate_preset(paths[count], &test_preset);
            PresetStatus health = check_preset_health(paths[count], &test_preset);

            if (health == STATUS_ERROR) {
                printf("%s%d. %s [CRITICAL ERROR]%s\n", clr_red, count + 1, names[count], clr_reset);
            } else if (health == STATUS_WARNING) {
                printf("%s%d. %s [MODIFIED/WARN]%s\n", clr_yellow, count + 1, names[count], clr_reset);
            } else {
                printf("%s%d. %s%s\n", clr_bold, count + 1, names[count], clr_reset);
            }
            count++;
        }
    }
    fclose(pf);

    if (count == 0) {
        printf("error: priority queue playlist is empty.\n");
        free(names); free(paths);
        exit(1);
    }

    printf("\nchoose preset (1-%d) or press ^f to inspect code: ", count);
    char choice = getch();

    // handle ctrl+f sequence (^f ascii code is 6).
    if (choice == 6) {
        printf("\nenter index number to preview code (1-%d): ", count);
        char preview_choice = getch();
        int p_idx = preview_choice - '1';

        if (p_idx >= 0 && p_idx < count) {
            show_preset_code_preview(names[p_idx], paths[p_idx]);
        }

        for (int i = 0; i < count; i++) { free(names[i]); free(paths[i]); }
        free(names); free(paths);

        get_preset_from_priority_menu(out_filename, max_len);
        return;
    }

    int idx = choice - '1';
    if (idx < 0 || idx >= count) {
        printf("\ninvalid selection. using highest priority index.\n");
        idx = 0;
    }

    BlurPreset final_check = {0};
    load_and_validate_preset(paths[idx], &final_check);
    PresetStatus run_status = check_preset_health(paths[idx], &final_check);

    if (run_status == STATUS_ERROR || has_unknown_keys(paths[idx])) {
        if (has_unknown_keys(paths[idx])) {
            show_critical_error_window("CONFIG CORRUPTION ERROR", "Found unexpected or malformed layout keys (like 'fggkk...='). Please clean up the file.");
        } else {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "Target encoder '%s' or backend '%s' is invalid/unsupported.", final_check.encoder, final_check.backend);
            show_critical_error_window("INVALID PIPELINE CONFIG", err_buf);
        }

        for (int i = 0; i < count; i++) { free(names[i]); free(paths[i]); }
        free(names); free(paths);

        get_preset_from_priority_menu(out_filename, max_len);
        return;
    }

    // safely export the exact absolute target file path to core launcher.
    strncpy(out_filename, paths[idx], max_len - 1);
    out_filename[max_len - 1] = '\0';

    // memory allocation cleanup.
    for (int i = 0; i < count; i++) { free(names[i]); free(paths[i]); }
    free(names); free(paths);
}

// void apply_preset_fallbacks(const char *preset_file, BlurPreset *preset) {
//     int was_corrupted = 0;
//
//     if (strlen(preset->name) == 0) {
//         strcpy(preset->name, "custom dynamic preset");
//         was_corrupted = 1;
//     }
//
//     if (strlen(preset->backend) == 0) {
//         printf("warning: backend configuration missing. falling back to ffmpeg engine.\n");
//         strcpy(preset->backend, "ffmpeg");
//         was_corrupted = 1;
//     }
//
//     if (strlen(preset->blur_mode) == 0) {
//         printf("warning: blur mode configuration missing. falling back to gaussian logic.\n");
//         strcpy(preset->blur_mode, "gaussian");
//         was_corrupted = 1;
//     }
//
//     if (preset->frames < 1 || preset->frames > 512) {
//         printf("warning: invalid frames range configuration. falling back to default 7 frames.\n");
//         preset->frames = 7;
//         was_corrupted = 1;
//     }
//
//     if (strlen(preset->output_fps) == 0) {
//         printf("warning: target output fps configuration missing. falling back to 60 frames.\n");
//         strcpy(preset->output_fps, "60");
//         was_corrupted = 1;
//     }
//
//     if (strlen(preset->encoder) == 0) {
//         printf("warning: video encoder configuration missing. falling back to safe libx264 codec.\n");
//         strcpy(preset->encoder, "libx264");
//         was_corrupted = 1;
//     }
//
//     if (strlen(preset->bitrate) == 0) {
//         printf("warning: bitrate flags missing. falling back to crf 14 execution.\n");
//         strcpy(preset->bitrate, "-crf 14");
//         was_corrupted = 1;
//     }
//
//     if (was_corrupted) {
//         printf("fixing and overwriting corrupted preset file.\n");
//         FILE *fw = fopen(preset_file, "w");
//         if (fw) {
//             fprintf(fw, "# fixed automatically by dildobean.\n\n");
//             fprintf(fw, "[preset]\n");
//             fprintf(fw, "name=%s\n", preset->name);
//             fprintf(fw, "backend=%s\n", preset->backend);
//             fprintf(fw, "frames=%d\n", preset->frames);
//             fprintf(fw, "blur_mode=%s\n", preset->blur_mode);
//             fprintf(fw, "output_fps=%s\n", preset->output_fps);
//             fprintf(fw, "encoder=%s\n", preset->encoder);
//             fprintf(fw, "bitrate=%s\n", preset->bitrate);
//             fclose(fw);
//         } else {
//             printf("warning: cannot overwrite template asset file.\n");
//         }
//     }
// }

void apply_preset_fallbacks(const char *preset_file, BlurPreset *preset) {
    if (validate_preset_structure(preset)) {
        printf("fixing and overwriting corrupted line(s) on disk...\n");
        patch_corrupted_lines(preset_file, preset);
    }
}

void load_and_validate_preset(const char *preset_file, BlurPreset *preset) {
    memset(preset->name, 0, sizeof(preset->name));
    memset(preset->backend, 0, sizeof(preset->backend));
    memset(preset->blur_mode, 0, sizeof(preset->blur_mode));
    memset(preset->output_fps, 0, sizeof(preset->output_fps));
    memset(preset->encoder, 0, sizeof(preset->encoder));
    memset(preset->bitrate, 0, sizeof(preset->bitrate));
    preset->frames = -1;
    preset->quality = -1;

    FILE *f = fopen(preset_file, "r");
    if (!f) {
        printf("warning: cannot open preset file. using safe defaults.\n");
        strcpy(preset->name, "custom dynamic preset");
        strcpy(preset->backend, "ffmpeg");
        preset->frames = 7;
        strcpy(preset->blur_mode, "equal");
        strcpy(preset->output_fps, "60");
        strcpy(preset->encoder, "libx264");
        strcpy(preset->bitrate, "-crf");
        preset->quality = 14;
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\n' || line[0] == '\r') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        while (*val == ' ' || *val == '\t') val++;
        trim_whitespace(val);
        trim_whitespace(key);

        if (strcmp(key, "name") == 0) strncpy(preset->name, val, 63);
        else if (strcmp(key, "backend") == 0) strncpy(preset->backend, val, 31);
        else if (strcmp(key, "blur_mode") == 0) strncpy(preset->blur_mode, val, 15);
        else if (strcmp(key, "output_fps") == 0) strncpy(preset->output_fps, val, 7);
        else if (strcmp(key, "encoder") == 0) strncpy(preset->encoder, val, 31);
        else if (strcmp(key, "bitrate") == 0) strncpy(preset->bitrate, val, 63);
        else if (strcmp(key, "quality") == 0) preset->quality = atoi(val);
        else if (strcmp(key, "frames") == 0) preset->frames = atoi(val);
    }
    fclose(f);

    apply_preset_fallbacks(preset_file, preset);
}
