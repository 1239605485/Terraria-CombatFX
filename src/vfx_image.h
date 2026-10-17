#ifndef COMBATFX_VFX_IMAGE_H
#define COMBATFX_VFX_IMAGE_H
#include <stddef.h>
#include <stdbool.h>
typedef struct { unsigned char *pixels; int width, height; size_t size; } FxImage;
bool fx_image_load(const char *path, int max_edge, FxImage *out);
void fx_image_free(FxImage *image);
#endif
