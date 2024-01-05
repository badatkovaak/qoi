#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "qoi.h"

typedef uint64_t un;
typedef uint8_t unchar;

// pixel generate_pixel_rgba(unchar mode) {
//     pixel res;
//     if (mode == 1) {
//         res = (pixel){
//             rand() % 16 + 120,
//             rand() % 16 + 120,
//             rand() % 16 + 120,
//             255,
//         };
//     } else if (mode == 2) {
//         res = (pixel){
//             rand() % 128 + 64,
//             rand() % 128 + 64,
//             rand() % 128 + 64,
//             255,
//         };
//     } else if (mode == 3) {
//         res = (pixel){
//             rand() % 256,
//             rand() % 256,
//             rand() % 256,
//             255,
//         };
//     }
//
//     return res;
// }
//
// pixel* generate_data_rgba(uint32_t width, uint32_t height, unchar mode) {
//     un size = width * height;
//     pixel* data = (Pixel*)malloc(size * 4);
//     for (un i = 0; i < size; i++) {
//         data[i] = generate_pixel_rgba(mode);
//     }
//     return data;
// }
//
// void print_image(const pixel* d, un size) {
//     for (un i = 0; i < size; i++) {
//         printf("r: %u, g: %u, b: %u, a: %u\n", d[i].r, d[i].g, d[i].b,
//         d[i].a);
//     }
// }

un write_image(const char* path, vector image) {
    FILE* f = fopen(path, "w");
    if (f == 0) return 0;
    un res = fwrite(image.data, 1, image.len, f);
    fclose(f);
    return res;
}

vector read_file_bin(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        vector r = {0, 0};
        return r;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unchar* buffer = (unchar*)malloc(size);

    fread(buffer, size, 1, f);
    fclose(f);

    vector res = {buffer, size};
    return res;
}

void print_vec(vector v) {
    for (un i = 0; i < v.len; i++) {
        printf("%x ", ((unchar*)v.data)[i]);
    }
    printf("\n");
}

// void print_buffer(pixel* buffer, size_t len) {
//     printf("\n");
//     for (un i = 0; i < len / 4; i++) {
//         for (un j = 0; j < 4; j++) {
//             printf("%02lu: %08x ", 4 * i + j, buffer[i * 4 + j]);
//         }
//         printf("\n");
//     }
//     printf("\n");
// }

int main() {
    printf("\n");

    qoi_desc desc = {0, 0, 0, 0};
    vector r1 = read_qoi("assets/qoi_logo.qoi", &desc);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    const qoi_desc desc1 = desc;
    printf("%lu\n", r1.len);
    vector r2 = qoi_decode(r1, &desc1);
    free(r1.data);
    vector r3 = qoi_encode(r2.data, &desc1);
    free(r2.data);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    un r = write_qoi("assets/img.qoi", r3, &desc);
    free(r3.data);

    printf("\n");
    return 0;
}
