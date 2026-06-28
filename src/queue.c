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

void show_preset_code_preview(const char *preset_name, const char *preset_filepath) {
    clear_screen();
    printf("name: %s\n", preset_name);
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
        if (action == 2) { // ^b (ctrl+b) - go back to core selection menu.
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

            // display clear human-readable preset names instead of raw paths.
            printf("%d. %s\n", count + 1, names[count]);
            count++;
        }
    }
    fclose(pf);

    if (count == 0) {
        printf("error: priority queue playlist is empty.\n");
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

        // clean up temporary dynamic memory before recursion loop.
        for (int i = 0; i < count; i++) { free(names[i]); free(paths[i]); }
        free(names); free(paths);

        // loop back into menu to pick the file for execution.
        get_preset_from_priority_menu(out_filename, max_len);
        return;
    }

    int idx = choice - '1';
    if (idx < 0 || idx >= count) {
        printf("\ninvalid selection. using highest priority index.\n");
        idx = 0;
    }

    // safely export the exact absolute target file path to core launcher.
    strncpy(out_filename, paths[idx], max_len - 1);
    out_filename[max_len - 1] = '\0';

    // memory allocation cleanup.
    for (int i = 0; i < count; i++) { free(names[i]); free(paths[i]); }
    free(names); free(paths);
}

void apply_preset_fallbacks(const char *preset_file, BlurPreset *preset) {
    int was_corrupted = 0;

    if (strlen(preset->name) == 0) {
        strcpy(preset->name, "custom dynamic preset");
        was_corrupted = 1;
    }

    if (strlen(preset->backend) == 0) {
        printf("warning: backend configuration missing. falling back to ffmpeg engine.\n");
        strcpy(preset->backend, "ffmpeg");
        was_corrupted = 1;
    }

    if (strlen(preset->blur_mode) == 0) {
        printf("warning: blur mode configuration missing. falling back to gaussian logic.\n");
        strcpy(preset->blur_mode, "gaussian");
        was_corrupted = 1;
    }

    if (preset->frames < 1 || preset->frames > 512) {
        printf("warning: invalid frames range configuration. falling back to default 7 frames.\n");
        preset->frames = 7;
        was_corrupted = 1;
    }

    if (strlen(preset->output_fps) == 0) {
        printf("warning: target output fps configuration missing. falling back to 60 frames.\n");
        strcpy(preset->output_fps, "60");
        was_corrupted = 1;
    }

    if (strlen(preset->encoder) == 0) {
        printf("warning: video encoder configuration missing. falling back to safe libx264 codec.\n");
        strcpy(preset->encoder, "libx264");
        was_corrupted = 1;
    }

    if (strlen(preset->bitrate) == 0) {
        printf("warning: bitrate flags missing. falling back to crf 14 execution.\n");
        strcpy(preset->bitrate, "-crf 14");
        was_corrupted = 1;
    }

    if (was_corrupted) {
        printf("fixing and overwriting corrupted preset file.\n");
        FILE *fw = fopen(preset_file, "w");
        if (fw) {
            fprintf(fw, "# fixed automatically by dildobean.\n\n");
            fprintf(fw, "[preset]\n");
            fprintf(fw, "name=%s\n", preset->name);
            fprintf(fw, "backend=%s\n", preset->backend);
            fprintf(fw, "frames=%d\n", preset->frames);
            fprintf(fw, "blur_mode=%s\n", preset->blur_mode);
            fprintf(fw, "output_fps=%s\n", preset->output_fps);
            fprintf(fw, "encoder=%s\n", preset->encoder);
            fprintf(fw, "bitrate=%s\n", preset->bitrate);
            fclose(fw);
        } else {
            printf("warning: cannot overwrite template asset file.\n");
        }
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
