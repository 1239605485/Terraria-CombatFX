#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "vendor/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "vfx_image.h"
bool fx_image_load(const char *path, int max_edge, FxImage *out) {
    if (!out) return false;
    memset(out,0,sizeof(*out));
    if (!path || max_edge < 16 || max_edge > 512) return false;
    FILE *file = fopen(path,"rb");
    if (!file) return false;
    if (fseek(file,0,SEEK_END) != 0) { fclose(file); return false; }
    long size = ftell(file);
    if (size <= 0 || size > 4*1024*1024 || fseek(file,0,SEEK_SET) != 0) { fclose(file); return false; }
    unsigned char *encoded = malloc((size_t)size);
    if (!encoded) { fclose(file); return false; }
    bool read_ok = fread(encoded,1,(size_t)size,file) == (size_t)size;
    fclose(file);
    int w = 0,h = 0,channels = 0;
    unsigned char *source = read_ok ? stbi_load_from_memory(encoded,(int)size,&w,&h,&channels,4) : NULL;
    free(encoded);
    if (!source || w <= 0 || h <= 0) { stbi_image_free(source); return false; }
    int largest = w > h ? w : h;
    int dw = largest > max_edge ? (w*max_edge+largest/2)/largest : w;
    int dh = largest > max_edge ? (h*max_edge+largest/2)/largest : h;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    unsigned char *dest = malloc((size_t)dw*dh*4);
    if (!dest) { stbi_image_free(source); return false; }
    // Box filter in premultiplied alpha space to prevent dark transparent fringes.
    for (int y = 0; y < dh; ++y) for (int x = 0; x < dw; ++x) {
        int x0 = x*w/dw, x1 = (x+1)*w/dw, y0 = y*h/dh, y1 = (y+1)*h/dh;
        if (x1 <= x0) x1 = x0+1;
        if (y1 <= y0) y1 = y0+1;
        uint64_t sum[4] = {0}; unsigned count = 0;
        for (int sy = y0; sy < y1; ++sy) for (int sx = x0; sx < x1; ++sx) {
            const unsigned char *p = source+((size_t)sy*w+sx)*4;
            sum[0] += p[0]*p[3]; sum[1] += p[1]*p[3]; sum[2] += p[2]*p[3]; sum[3] += p[3]; ++count;
        }
        unsigned char *p = dest+((size_t)y*dw+x)*4;
        for (unsigned c = 0; c < 3; ++c) p[c] = (unsigned char)(sum[c]/(255*count));
        p[3] = (unsigned char)(sum[3]/count);
    }
    stbi_image_free(source);
    out->pixels = dest; out->width = dw; out->height = dh; out->size = (size_t)dw*dh*4;
    return true;
}
void fx_image_free(FxImage *image) {
    if (!image) return;
    free(image->pixels); memset(image,0,sizeof(*image));
}
