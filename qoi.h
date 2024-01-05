#ifndef QOI_H
#define QOI_H

#include <stdint.h>

typedef unsigned char unchar;
typedef uint64_t un;

typedef enum {
    QOI_OP_INDEX = 0x00,
    QOI_OP_DIFF = 0x40,
    QOI_OP_LUMA = 0x80,
    QOI_OP_RUN = 0xc0,
    QOI_OP_RGB = 0xfe,
    QOI_OP_RGBA = 0xff
} qoi_op;

typedef struct {
    void* data;
    un len;
} Vector;

typedef struct {
    uint32_t width, height;
    uint8_t channels, colorspace;
} qoi_desc;

typedef struct {
    unchar r, g, b;
} Pixel_rgb;

typedef struct {
    unchar r, g, b, a;
} Pixel;

Vector qoi_encode(const void* image, const qoi_desc* desc);

Vector qoi_decode(Vector image, const qoi_desc* desc);

un write_qoi(const char* path, Vector data, const qoi_desc* desc);

Vector read_qoi(const char* path, void* desc);

uint32_t switch_endianness(uint32_t value);

#endif
