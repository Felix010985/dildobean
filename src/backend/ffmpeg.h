#ifndef FFMPEG_H
#define FFMPEG_H

#include "../queue.h"

void run_ffmpeg_backend(const char *input_file, const char *output_file, const BlurPreset *preset);

#endif
