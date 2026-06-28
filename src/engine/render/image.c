// Image implementation using stb_image (included into mach.c).

#include "image.h"
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image.h"

Image image_load(const char *path) {
    Image img = {0};
    if (!path) return img;

    int w, h, channels;
    u8 *data = stbi_load(path, &w, &h, &channels, 4);  // Force RGBA
    if (!data) return img;

    img.data = data;
    img.width = w;
    img.height = h;
    img.channels = 4;
    return img;
}

void image_free(Image *img) {
    if (!img || !img->data) return;
    stbi_image_free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
}
