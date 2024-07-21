#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "qoi.h"

typedef uint64_t un;
typedef uint8_t unchar;

typedef struct {
    unchar r, g, b;
} Pixel_rgb;

typedef struct {
    unchar r, g, b, a;
} Pixel;

Pixel generate_Pixel_rgba(unchar mode) {
    Pixel res;
    if (mode == 1) {
        res = (Pixel){
            rand() % 16 + 120,
            rand() % 16 + 120,
            rand() % 16 + 120,
            255,
        };
    } else if (mode == 2) {
        res = (Pixel){
            rand() % 128 + 64,
            rand() % 128 + 64,
            rand() % 128 + 64,
            255,
        };
    } else if (mode == 3) {
        res = (Pixel){
            rand() % 256,
            rand() % 256,
            rand() % 256,
            255,
        };
    }

    return res;
}

Pixel* generate_data_rgba(uint32_t width, uint32_t height, unchar mode) {
    un size = width * height;
    Pixel* data = malloc(size * 4);
    for (un i = 0; i < size; i++) {
        data[i] = generate_Pixel_rgba(mode);
    }
    return data;
}

void print_image(const Pixel* d, un size) {
    for (un i = 0; i < size; i++) {
        printf("r: %u, g: %u, b: %u, a: %u\n", d[i].r, d[i].g, d[i].b, d[i].a);
    }
}

un write_image(const char* path, Vector image) {
    FILE* f = fopen(path, "w");
    if (f == 0) return 0;
    un res = fwrite(image.data, 1, image.len, f);
    fclose(f);
    return res;
}

Vector read_file_bin(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        Vector r = {0, 0};
        return r;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unchar* buffer = (unchar*)malloc(size);

    fread(buffer, size, 1, f);
    fclose(f);

    Vector res = {buffer, size};
    return res;
}

void print_vec(Vector v) {
    for (un i = 0; i < v.len; i++) {
        printf("%x ", ((unchar*)v.data)[i]);
    }
    printf("\n");
}

void print_buffer(Pixel* buffer, size_t len) {
    printf("\n");
    for (un i = 0; i < len / 4; i++) {
        for (un j = 0; j < 4; j++) {
            printf("%02lu: %08x ", 4 * i + j, buffer[i * 4 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
    printf("\n");

    qoi_desc desc = {0, 0, 0, 0};
    Vector r1 = read_qoi("assets/baboon.qoi", &desc);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    const qoi_desc desc1 = desc;
    // printf("%lu\n", r1.len);
    Vector r2 = qoi_decode(r1, &desc1);
    write_image("assets/img.bin", r2);
    free(r1.data);
    Vector r3 = qoi_encode(r2.data, &desc1);
    free(r2.data);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    un r = write_qoi("assets/img.qoi", r3, &desc);
    free(r3.data);

    qoi_desc d1 = {256, 256, 4, 7};
    Vector v1 = read_file_bin("assets/img1.bin");
    Vector v2 = qoi_encode(v1.data, &d1);
    write_qoi("assets/img1.qoi", v2, &d1);

    printf("\n");
    return 0;
}
