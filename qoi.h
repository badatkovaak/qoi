#ifndef QOI_H
#define QOI_H

#include <stdint.h>

typedef struct {
    void* data;
    unsigned long len;
} vector;

typedef struct {
    uint32_t width, height;
    uint8_t channels, colorspace;
} qoi_desc;

vector qoi_encode(const void* image, const qoi_desc* desc);

vector qoi_decode(vector image, const qoi_desc* desc);

unsigned long write_qoi(const char* path, vector data, const qoi_desc* desc);

vector read_qoi(const char* path, void* desc);

uint32_t switch_endianness(uint32_t value);

#endif
