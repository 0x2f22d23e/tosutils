#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR

#include "dither.h"

#include "stb_image.h"
#include <math.h>
#include <unistd.h>

#define SIMPLEWEBP_IMPLEMENTATION

#include "simplewebp.h"

#define RED 4
#define GREEN 2
#define BLUE 1
#define COLORS_NUM 16

struct CDC {
    uint64_t cdt;
    int32_t x0, y0, width, width_internal, height, flags;
    unsigned char* body;
};

unsigned char rgba_to_4_bit(int r, int g, int b, int a)
{
    if (a < 0xFF)
        return 0xFF;
    unsigned char res = 0;
    if (sqrt(r) + sqrt(g) + sqrt(b) >= sqrt(0x80)) {
        res = 8;
        if (r >= 0x80)
            res |= RED;
        if (g >= 0x80)
            res |= GREEN;
        if (b >= 0x80)
            res |= BLUE;
    } else {
        if (r >= 0x40)
            res |= RED;
        if (g >= 0x40)
            res |= GREEN;
        if (b >= 0x40)
            res |= BLUE;
    }
    return res;
}

bool ends_with(char* str, char* marker)
{
    if (!str || !marker)
        return false;

    size_t str_len = strlen(str);
    size_t marker_len = strlen(marker);

    if (marker_len > str_len)
        return false;

    return strcmp(str + str_len - marker_len, marker) == 0;
}

unsigned char* swebp_load_from_file(FILE* f, int* w, int* h)
{
    simplewebp* s = NULL;
    int res = simplewebp_load_from_file(f, NULL, &s);
    if (res) {
        printf("%s\n", simplewebp_get_error_text(res));
        return NULL;
    }
    if (!s) {
        return NULL;
    }
    size_t _w, _h;
    simplewebp_get_dimensions(s, &_w, &_h);
    if (!_w || !_h) {
        return NULL;
    }
    *w = _w;
    *h = _h;
    unsigned char* pixels = calloc((_w * _h) * 4, 1);
    res = simplewebp_decode(s, pixels, NULL);
    if (res) {
        printf("%s\n", simplewebp_get_error_text(res));
        free(pixels);
        return NULL;
    }
    return pixels;
}

char* output_filename(char* newfn, char* oldfn)
{
    int len = strlen(oldfn);
    while (oldfn[len] != '.')
        len--;
    memcpy(newfn, oldfn, len);
    strcpy(newfn + len, ".GR");
    return newfn;
}

int write_gr_file(char* filename, unsigned char* pixels, int w, int h)
{
    struct CDC* dc = calloc(sizeof(struct CDC), 1);
    dc->width = w;
    dc->height = h;
    dc->width_internal = (w + 7) & (-8);
    dc->body = calloc(dc->width_internal * dc->height, 1);
    int i = 0, x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            dc->body[(y * dc->width_internal) + x] = rgba_to_4_bit(pixels[i], pixels[i + 1], pixels[i + 2], pixels[i + 3]);
            i += 4;
        }
    char outfile[512];
    FILE* f = fopen(output_filename((char*)&outfile, filename), "wb");
    fwrite(dc, sizeof(struct CDC) - sizeof(unsigned char*), 1, f);
    fwrite(dc->body, dc->height * dc->width_internal, 1, f);
    fclose(f);
    free(dc->body);
    free(dc);
    return 0;
}

int image2GR(char* filename)
{
    int res = access(filename, F_OK);
    if (res) {
        printf("File not found: %s\n", filename);
        return res;
    }
    FILE* f = fopen(filename, "rb");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = ends_with(filename, ".webp") ? swebp_load_from_file(f, &w, &h) : stbi_load_from_file(f, &w, &h, &ch, 4);
    fclose(f);
    if (!pixels) {
        printf("Error decoding pixels\n");
        return 1;
    }
    render_4bit_floydstein((uint32_t*)pixels, w, h);
    write_gr_file(filename, pixels, w, h);
    free(pixels);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: image2GR file(s)\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        int res = image2GR(argv[i]);
        if (res) {
            return res;
        }
    }
    return 0;
}