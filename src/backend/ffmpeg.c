#include "ffmpeg.h"
#include "../adapt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void build_gaussian_weights_string(int n, char *out_buf, size_t buf_sz) {
    double sigma = n / 6.0;
    double center = (n - 1) / 2.0;
    double *w = malloc(n * sizeof(double));
    double sum = 0.0;

    for (int i = 0; i < n; i++) {
        double x = i - center;
        w[i] = exp(-(x * x) / (2 * sigma * sigma));
        sum += w[i];
    }

    size_t pos = 0;
    for (int i = 0; i < n; i++) {
        int wi = (int)round((w[i] / sum) * 1000);
        pos += snprintf(out_buf + pos, buf_sz - pos, "%d%s", wi, (i < n - 1) ? "|" : "");
    }

    free(w);
}

void run_ffmpeg_backend(const char *input_file, const char *output_file, const BlurPreset *preset) {
    char filter[2048];
    char weights[1024];

    if (strcmp(preset->blur_mode, "equal") == 0) {
        strcpy(weights, "1");
    } else {
        build_gaussian_weights_string(preset->frames, weights, sizeof(weights));
    }

    snprintf(filter, sizeof(filter), "tmix=frames=%d:weights=%s,fps=%s",
             preset->frames, weights, preset->output_fps);

    printf("launching ffmpeg pipeline.\n");
    printf("filter config: %s\n", filter);

    int status = _spawnlp(0, "ffmpeg", "ffmpeg",
                      "-loglevel", "quiet",
                      "-stats",
                      "-i", input_file,
                      "-vf", filter,
                      "-c:a", "copy",
                      "-c:v", preset->encoder,
                      preset->bitrate,
                      "-y", output_file,
                      NULL);

    if (status == 0) {
        printf("\nffmpeg rendering finished successfully.\n");
    } else {
        printf("\nerror: ffmpeg pipeline returned status %d.\n", status);
    }
}
