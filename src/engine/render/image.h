// Image loading: PNG, BMP, JPG support via stb_image.

#ifndef IMAGE_H
#define IMAGE_H

#include "../base/base.h"

typedef struct {
    u8 *data;        // Raw pixel data (RGBA)
    i32 width;
    i32 height;
    i32 channels;    // Usually 4 (RGBA)
} Image;

// Load image from file. Returns image with allocated data, or zeroed struct on failure.
Image image_load(const char *path);

// Free image data.
void image_free(Image *img);

#endif
