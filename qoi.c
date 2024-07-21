#include "qoi.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define cond_eq(p, c) \
    (p.r == c.r) && (p.g == c.g) && (p.b == c.b) && (p.a == c.a)

#define hash(c) (c.r * 3 + c.g * 5 + c.b * 7 + c.a * 11) % 64

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
    unchar r, g, b;
} pixel_rgb;

typedef struct {
    unchar r, g, b, a;
} Pixel;

uint32_t switch_endianness(uint32_t value) {
    return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) |
           ((value << 8) & 0xFF0000) | ((value << 24) & 0xFF000000);
}

bool cond_diff(Pixel curr, Pixel prev) {
    char dr = curr.r - prev.r;
    char dg = curr.g - prev.g;
    char db = curr.b - prev.b;
    bool cond_r = dr + 2 >= 0 && dr <= 1;
    bool cond_g = dg + 2 >= 0 && dg <= 1;
    bool cond_b = db + 2 >= 0 && db <= 1;
    return cond_r && cond_g && cond_b && curr.a == prev.a;
}

bool cond_luma(Pixel curr, Pixel prev) {
    char dg = curr.g - prev.g;
    char dr_dg = curr.r - prev.r - dg;
    char db_dg = curr.b - prev.b - dg;
    bool cond_g = dg + 32 >= 0 && dg <= 31;
    bool cond_r = dr_dg + 8 >= 0 && dr_dg <= 7;
    bool cond_b = db_dg + 8 >= 0 && db_dg <= 7;
    return cond_r && cond_g && cond_b && curr.a == prev.a;
}

Vector qoi_encode(const void* image, const qoi_desc* desc) {
    Pixel* data = (Pixel*)image;

    Pixel* buffer = malloc(64 * 4);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel current = {0, 0, 0, 255};
    Pixel previous = current;

    un curr_index = 0;
    un img_size = desc->height * desc->width;
    unchar res;

    unchar* output = malloc(img_size * 5);
    un out_len = 0;

    while (curr_index < img_size) {
        previous = current;
        current = data[curr_index];
        if (curr_index > 0) {
            buffer[hash(previous)] = previous;
        }

        if (cond_eq(current, previous)) {
            un run_len = 1;
            Pixel previous_1 = current;
            Pixel current_1;

            if (curr_index + 1 < img_size) {
                Pixel current_1 = data[curr_index + run_len];
            } else {
                output[out_len] = QOI_OP_RUN;
                out_len++;
                curr_index++;
                continue;
            }

            while (cond_eq(previous_1, current_1) && run_len < 62 &&
                   curr_index + run_len + 1 < img_size) {
                run_len += 1;
                previous_1 = current_1;
                current_1 = data[curr_index + run_len];
            }

            output[out_len] = QOI_OP_RUN + run_len - 1;
            out_len++;
            curr_index += run_len - 1;
        } else if (cond_eq(current, buffer[hash(current)])) {
            output[out_len] = hash(current);
            out_len++;
        } else if (cond_diff(current, previous)) {
            res = QOI_OP_DIFF + ((current.r + 2 - previous.r) << 4) +
                  ((current.g + 2 - previous.g) << 2) + current.b + 2 -
                  previous.b;
            output[out_len] = res;
            out_len++;
        } else if (cond_luma(current, previous)) {
            char dg = current.g - previous.g;
            char dr_dg = current.r - previous.r - dg;
            char db_dg = current.b - previous.b - dg;
            output[out_len] = QOI_OP_LUMA + dg + 32;
            output[out_len + 1] = ((dr_dg + 8) << 4) + db_dg + 8;
            out_len += 2;
        } else if (current.a == previous.a) {
            output[out_len] = QOI_OP_RGB;
            output[out_len + 1] = current.r;
            output[out_len + 2] = current.g;
            output[out_len + 3] = current.b;
            out_len += 4;
        } else {
            output[out_len] = QOI_OP_RGBA;
            output[out_len + 1] = current.r;
            output[out_len + 2] = current.g;
            output[out_len + 3] = current.b;
            output[out_len + 4] = current.a;
            out_len += 5;
        }
        curr_index++;
    }

    free(buffer);
    return (Vector){realloc(output, out_len), out_len};
}

un decode_byte(unchar b) {
    if (b == QOI_OP_RGB) {
        return QOI_OP_RGB;
    } else if (b == QOI_OP_RGBA) {
        return QOI_OP_RGBA;
    } else if ((b >> 6) == (QOI_OP_INDEX >> 6)) {
        return QOI_OP_INDEX;
    } else if ((b >> 6) == 1) {
        return QOI_OP_DIFF;
    } else if ((b >> 6) == 2) {
        return QOI_OP_LUMA;
    } else if ((b >> 6) == 3) {
        return QOI_OP_RUN;
    }
    return 1;
}

Vector qoi_decode(Vector image, const qoi_desc* desc) {
    un curr_index = 0;
    unchar* data = (unchar*)image.data;

    Pixel* buffer = malloc(sizeof(Pixel) * 64);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel previous = {0, 0, 0, 255};

    un out_size = desc->width * desc->height * 4;
    Pixel* output = (Pixel*)malloc(out_size);

    un output_index = 0;
    unchar byte1;
    un run;

    while (curr_index < image.len) {
        switch (decode_byte(data[curr_index])) {
            case 1:
                free(buffer);
                free(output);
                printf("Incorrect byte sequence!\n");
                return (Vector){0, 0};
            case QOI_OP_INDEX:
                previous = buffer[data[curr_index]];
                output[output_index] = previous;
                output_index++;
                break;
            case QOI_OP_DIFF:
                byte1 = data[curr_index];
                previous = (Pixel){.r = previous.r + ((byte1 >> 4) - 6),
                                   .g = previous.g + ((byte1 >> 2) % 4 - 2),
                                   .b = previous.b + (byte1 % 4 - 2),
                                   .a = previous.a};
                output[output_index] = previous;
                output_index++;
                break;
            case QOI_OP_LUMA:
                byte1 = data[curr_index];
                unchar byte2 = data[curr_index + 1];
                char diff_g = byte1 - QOI_OP_LUMA - 32;
                previous = (Pixel){.r = previous.r + diff_g + (byte2 >> 4) - 8,
                                   .g = previous.g + diff_g,
                                   .b = previous.b + diff_g + byte2 % 16 - 8,
                                   .a = previous.a};
                output[output_index] = previous;
                output_index++;
                curr_index++;
                break;
            case QOI_OP_RUN:
                run = data[curr_index] - QOI_OP_RUN + 1;
                for (un i = 0; i < run; i++) {
                    output[output_index + i] = previous;
                }
                output_index += run;
                break;
            case QOI_OP_RGB:
                previous = (Pixel){.r = data[curr_index + 1],
                                   .g = data[curr_index + 2],
                                   .b = data[curr_index + 3],
                                   .a = previous.a};
                output[output_index] = previous;
                output_index++;
                curr_index += 3;
                break;
            case QOI_OP_RGBA:
                previous = (Pixel){.r = data[curr_index + 1],
                                   .g = data[curr_index + 2],
                                   .b = data[curr_index + 3],
                                   .a = data[curr_index + 4]};
                output[output_index] = previous;
                output_index++;
                curr_index += 4;
                break;
        }
        buffer[hash(previous)] = previous;
        curr_index++;
    }
    free((void*)buffer);
    return (Vector){realloc(output, output_index * 4), output_index * 4};
}

un write_qoi(const char* path, Vector data, const qoi_desc* desc) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        printf("Error!\n");
        return 0;
    }

    const char* magic = "qoif";
    un mg = fwrite(magic, 1, 4, f);
    un header = fwrite(desc, 1, 10, f);
    un res = fwrite(data.data, 1, data.len, f);
    uint64_t one = 1ULL << (8 * 7);
    un on = fwrite(&one, 1, 8, f);
    fclose(f);
    return mg + header + res + on;
}

Vector read_qoi(const char* path, void* desc) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return (Vector){0, 0};
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* magic = malloc(4);
    un magic_s = fread(magic, 1, 4, f);

    if (strncmp(magic, "qoif", 4)) {
        printf("Not a .qoi file");
        free(magic);
        return (Vector){0, 0};
    }
    free(magic);

    un desc_s = fread(desc, 1, 10, f);
    un img_size = size - 22;
    void* image = malloc(img_size);
    un image_s = fread(image, img_size, 1, f);

    fclose(f);
    return (Vector){image, img_size};
}
