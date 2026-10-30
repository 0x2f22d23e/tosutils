#include <stdint.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

#define COLORS_NUM 16

struct CDC {
    uint64_t cdt;
    int32_t x0, y0, width, width_internal, height, flags;
    unsigned char* body;
};

struct CDC* GRRead(char const* filename)
{
    struct CDC* dc = malloc(sizeof(struct CDC));
    FILE* f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size);
    fread(buf, 1, size, f);
    fclose(f);
    memcpy(dc, buf, sizeof(struct CDC));
    dc->body = buf + sizeof(struct CDC) - 8;
    return dc;
}

uint32_t gr_palette_std[COLORS_NUM] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAAAA00,
    0x0000AA, 0xAA00AA, 0x0055AA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
    0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
};

uint32_t GrPeek(struct CDC* dc, int x, int y)
{
    int index = dc->body[(y * dc->width_internal) + x];
    return index > 15 ? 0 : gr_palette_std[index] | (0xff << 24);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: %s file.GR\n", argv[0]);
        exit(1);
    }
    char* filename = argv[1];
    if (access(filename, F_OK)) {
        printf("file not found: %s\n", filename);
        exit(1);
    }
    struct CDC* dc = GRRead(filename);
    if (!dc) {
        printf("error reading file: %s\n", filename);
        exit(1);
    }
    int i = 0;
    uint32_t* data = malloc((dc->width * dc->height) * sizeof(uint32_t));
    for (int y = 0; y < dc->height; y++) {
        for (int x = 0; x < dc->width; x++) {
            data[i++] = GrPeek(dc, x, y);
        }
    }
    strcpy(dc->body, filename);
    strcpy(dc->body + strlen(filename), ".png");
    stbi_write_png(dc->body, dc->width, dc->height, 4, data, dc->width * sizeof(uint32_t));
    free(data);
    free(dc->body - sizeof(struct CDC) + 8);
    free(dc);
    exit(0);
}
