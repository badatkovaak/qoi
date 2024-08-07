#ifndef QOI_H
#define QOI_H

#include <stdint.h>

typedef struct {
    void* data;
    unsigned long len;
} Vector;

typedef struct {
    uint32_t width, height;
    uint8_t channels, colorspace;
} QoiDescription;

Vector qoi_encode(const void* image, const QoiDescription* desc);

Vector qoi_decode(Vector image, const QoiDescription* desc);

unsigned long write_qoi(const char* path, Vector data,
                        const QoiDescription* desc);

Vector read_qoi(const char* path, void* desc);

uint32_t switch_endianness(uint32_t value);

#endif  // !QOI_H
