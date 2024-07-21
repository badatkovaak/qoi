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

bool condition_diff(Pixel curr, Pixel prev) {
    char dr = curr.r - prev.r;
    char dg = curr.g - prev.g;
    char db = curr.b - prev.b;

    bool cond_r = dr + 2 >= 0 && dr <= 1;
    bool cond_g = dg + 2 >= 0 && dg <= 1;
    bool cond_b = db + 2 >= 0 && db <= 1;

    return cond_r && cond_g && cond_b && curr.a == prev.a;
}

bool condition_luma(Pixel curr, Pixel prev) {
    char dg = curr.g - prev.g;
    char dr_dg = curr.r - prev.r - dg;
    char db_dg = curr.b - prev.b - dg;

    bool cond_g = dg + 32 >= 0 && dg <= 31;
    bool cond_r = dr_dg + 8 >= 0 && dr_dg <= 7;
    bool cond_b = db_dg + 8 >= 0 && db_dg <= 7;

    return cond_r && cond_g && cond_b && curr.a == prev.a;
}

Vector qoi_encode(const void* image, const QoiDescription* desc) {
    Pixel* data = (Pixel*)image;

    Pixel* running_buffer = malloc(64 * 4);
    for (un i = 0; i < 64; i++) {
        running_buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel current_pixel = {0, 0, 0, 255};
    Pixel previous_pixel = current_pixel;

    un current_index = 0;
    un image_size = desc->height * desc->width;
    unchar res;

    unchar* output = malloc(image_size * 5);
    un output_index = 0;

    while (current_index < image_size) {
        previous_pixel = current_pixel;
        current_pixel = data[current_index];
        if (current_index > 0) {
            running_buffer[hash(previous_pixel)] = previous_pixel;
        }

        if (cond_eq(current_pixel, previous_pixel)) {
            un run_length = 1;
            Pixel previous_pixel2 = current_pixel;
            Pixel current_pixel2;

            if (current_index + 1 < image_size) {
                Pixel current_1 = data[current_index + run_length];
            } else {
                output[output_index] = QOI_OP_RUN;
                output_index++;
                current_index++;
                continue;
            }

            while (cond_eq(previous_pixel2, current_pixel2) &&
                   run_length < 62 &&
                   current_index + run_length + 1 < image_size) {
                run_length += 1;
                previous_pixel2 = current_pixel2;
                current_pixel2 = data[current_index + run_length];
            }

            output[output_index] = QOI_OP_RUN + run_length - 1;
            output_index++;
            current_index += run_length - 1;
        } else if (cond_eq(current_pixel,
                           running_buffer[hash(current_pixel)])) {
            output[output_index] = hash(current_pixel);
            output_index++;
        } else if (condition_diff(current_pixel, previous_pixel)) {
            output[output_index] =
                QOI_OP_DIFF + ((current_pixel.r + 2 - previous_pixel.r) << 4) +
                ((current_pixel.g + 2 - previous_pixel.g) << 2) +
                current_pixel.b + 2 - previous_pixel.b;
            output_index++;
        } else if (condition_luma(current_pixel, previous_pixel)) {
            char delta_g = current_pixel.g - previous_pixel.g;
            char delta_r_delta_g = current_pixel.r - previous_pixel.r - delta_g;
            char delta_b_delta_g = current_pixel.b - previous_pixel.b - delta_g;

            output[output_index] = QOI_OP_LUMA + delta_g + 32;
            output[output_index + 1] =
                ((delta_r_delta_g + 8) << 4) + delta_b_delta_g + 8;
            output_index += 2;
        } else if (current_pixel.a == previous_pixel.a) {
            output[output_index] = QOI_OP_RGB;
            output[output_index + 1] = current_pixel.r;
            output[output_index + 2] = current_pixel.g;
            output[output_index + 3] = current_pixel.b;
            output_index += 4;
        } else {
            output[output_index] = QOI_OP_RGBA;
            output[output_index + 1] = current_pixel.r;
            output[output_index + 2] = current_pixel.g;
            output[output_index + 3] = current_pixel.b;
            output[output_index + 4] = current_pixel.a;
            output_index += 5;
        }
        current_index++;
    }

    free(running_buffer);
    return (Vector){realloc(output, output_index), output_index};
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

Vector qoi_decode(Vector image, const QoiDescription* description) {
    un current_index = 0;
    unchar* image_data = (unchar*)image.data;

    Pixel* running_buffer = malloc(sizeof(Pixel) * 64);
    for (un i = 0; i < 64; i++) {
        running_buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel previous_pixel = {0, 0, 0, 255};

    Pixel* output = malloc(description->width * description->height * 4);

    un output_index = 0;
    unchar byte_to_decode;
    un run_length;

    while (current_index < image.len) {
        switch (decode_byte(image_data[current_index])) {
            case 1:
                free(running_buffer);
                free(output);
                return (Vector){0, 0};
            case QOI_OP_INDEX:
                previous_pixel = running_buffer[image_data[current_index]];
                output[output_index] = previous_pixel;
                output_index++;
                break;
            case QOI_OP_DIFF:
                byte_to_decode = image_data[current_index];
                previous_pixel = (Pixel){
                    .r = previous_pixel.r + ((byte_to_decode >> 4) - 6),
                    .g = previous_pixel.g + ((byte_to_decode >> 2) % 4 - 2),
                    .b = previous_pixel.b + (byte_to_decode % 4 - 2),
                    .a = previous_pixel.a};
                output[output_index] = previous_pixel;
                output_index++;
                break;
            case QOI_OP_LUMA:
                byte_to_decode = image_data[current_index];
                unchar byte_to_decode2 = image_data[current_index + 1];
                char delta_g = byte_to_decode - QOI_OP_LUMA - 32;
                previous_pixel = (Pixel){
                    .r =
                        previous_pixel.r + delta_g + (byte_to_decode2 >> 4) - 8,
                    .g = previous_pixel.g + delta_g,
                    .b = previous_pixel.b + delta_g + byte_to_decode2 % 16 - 8,
                    .a = previous_pixel.a};
                output[output_index] = previous_pixel;
                output_index++;
                current_index++;
                break;
            case QOI_OP_RUN:
                run_length = image_data[current_index] - QOI_OP_RUN + 1;
                for (un i = 0; i < run_length; i++) {
                    output[output_index + i] = previous_pixel;
                }
                output_index += run_length;
                break;
            case QOI_OP_RGB:
                previous_pixel = (Pixel){.r = image_data[current_index + 1],
                                         .g = image_data[current_index + 2],
                                         .b = image_data[current_index + 3],
                                         .a = previous_pixel.a};
                output[output_index] = previous_pixel;
                output_index++;
                current_index += 3;
                break;
            case QOI_OP_RGBA:
                previous_pixel = (Pixel){.r = image_data[current_index + 1],
                                         .g = image_data[current_index + 2],
                                         .b = image_data[current_index + 3],
                                         .a = image_data[current_index + 4]};
                output[output_index] = previous_pixel;
                output_index++;
                current_index += 4;
                break;
        }

        running_buffer[hash(previous_pixel)] = previous_pixel;
        current_index++;
    }

    free((void*)running_buffer);
    return (Vector){realloc(output, output_index * 4), output_index * 4};
}

un write_qoi(const char* path, Vector data, const QoiDescription* description) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return 0;
    }

    const char* magic_bytes = "qoif";
    un magic_bytes_written = fwrite(magic_bytes, 1, 4, f);
    un description_written = fwrite(description, 1, 10, f);
    un image_written = fwrite(data.data, 1, data.len, f);
    uint64_t one = 1ULL << (8 * 7);
    un ending_written = fwrite(&one, 1, 8, f);

    fclose(f);
    return magic_bytes_written + description_written + image_written +
           ending_written;
}

Vector read_qoi(const char* path, void* description) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return (Vector){0, 0};
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* magic_bytes = malloc(4);
    un magic_bytes_read = fread(magic_bytes, 1, 4, f);

    if (strncmp(magic_bytes, "qoif", 4)) {
        free(magic_bytes);
        fclose(f);
        return (Vector){0, 0};
    }
    free(magic_bytes);

    un description_read = fread(description, 1, 10, f);
    un image_size = size - 22;
    void* image = malloc(image_size);
    un image_read = fread(image, image_size, 1, f);

    fclose(f);
    return (Vector){image, image_size};
}
