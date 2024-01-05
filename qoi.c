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
    unchar op;
    unchar r;
    unchar g;
    unchar b;
    unchar a;
} full_rep;

typedef struct {
    void* data;
    un len;
} Vector;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
} qoi_desc;

typedef struct {
    unchar r;
    unchar g;
    unchar b;
} Pixel_rgb;

typedef struct {
    unchar r;
    unchar g;
    unchar b;
    unchar a;
} Pixel;

uint32_t switch_endianness(uint32_t value) {
    return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) |
           ((value << 8) & 0xFF0000) | ((value << 24) & 0xFF000000);
}

uint64_t swi_end(uint64_t value) {
    return ((value & 0x00000000000000FF) << 56) |
           ((value & 0x000000000000FF00) << 40) |
           ((value & 0x0000000000FF0000) << 24) |
           ((value & 0x00000000FF000000) << 8) |
           ((value & 0x000000FF00000000) >> 8) |
           ((value & 0x0000FF0000000000) >> 24) |
           ((value & 0x00FF000000000000) >> 40) |
           ((value & 0xFF00000000000000) >> 56);
}

bool cond_diff(Pixel c, Pixel p) {
    char dr = c.r - p.r;
    char dg = c.g - p.g;
    char db = c.b - p.b;
    bool cond_r = dr + 2 >= 0 && dr <= 1;
    bool cond_g = dg + 2 >= 0 && dg <= 1;
    bool cond_b = db + 2 >= 0 && db <= 1;
    if (cond_r && cond_g && cond_b && c.a == p.a) {
        return true;
    }
    return false;
}

bool cond_luma(Pixel c, Pixel p) {
    char dg = c.g - p.g;
    char dr_dg = c.r - p.r - dg;
    char db_dg = c.b - p.b - dg;
    bool cond_g = dg + 32 >= 0 && dg <= 31;
    bool cond_r = dr_dg + 8 >= 0 && dr_dg <= 7;
    bool cond_b = db_dg + 8 >= 0 && db_dg <= 7;
    if (cond_r && cond_g && cond_b && c.a == p.a) {
        return true;
    }
    return false;
}

// Pixel generate_pixel_rgba(unchar mode) {
//     Pixel res;
//     if (mode == 1) {
//         res = (Pixel){
//             rand() % 16 + 120,
//             rand() % 16 + 120,
//             rand() % 16 + 120,
//             255,
//         };
//     } else if (mode == 2) {
//         res = (Pixel){
//             rand() % 128 + 64,
//             rand() % 128 + 64,
//             rand() % 128 + 64,
//             255,
//         };
//     } else if (mode == 3) {
//         res = (Pixel){
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
// Pixel* generate_data_rgba(uint32_t width, uint32_t height, unchar mode) {
//     un size = width * height;
//     Pixel* data = (Pixel*)malloc(size * 4);
//     for (un i = 0; i < size; i++) {
//         data[i] = generate_pixel_rgba(mode);
//     }
//     return data;
// }
//
// void print_image(const Pixel* d, un size) {
//     for (un i = 0; i < size; i++) {
//         printf("r: %u, g: %u, b: %u, a: %u\n", d[i].r, d[i].g, d[i].b,
//         d[i].a);
//     }
// }
//
// Vector read_file_bin(const char* path) {
//     FILE* f = fopen(path, "rb");
//     if (f == NULL) {
//         Vector r = {0, 0};
//         return r;
//     }
//
//     fseek(f, 0, SEEK_END);
//     size_t size = ftell(f);
//     fseek(f, 0, SEEK_SET);
//
//     unchar* buffer = (unchar*)malloc(size);
//
//     fread(buffer, size, 1, f);
//     fclose(f);
//
//     Vector res = {buffer, size};
//     return res;
// }

un write_image(const char* path, Vector image) {
    FILE* f = fopen(path, "w");
    if (f == 0) return 0;
    un res = fwrite(image.data, 1, image.len, f);
    fclose(f);
    return res;
}

// void print_vec(Vector v) {
//     for (un i = 0; i < v.len; i++) {
//         printf("%x ", ((unchar*)v.data)[i]);
//     }
//     printf("\n");
// }
//
// void print_buffer(Pixel* buffer, size_t len) {
//     printf("\n");
//     for (un i = 0; i < len / 4; i++) {
//         for (un j = 0; j < 4; j++) {
//             printf("%02lu: %08x ", 4 * i + j, buffer[i * 4 + j]);
//         }
//         printf("\n");
//     }
//     printf("\n");
// }

Vector qoi_encode(const void* image, const qoi_desc* desc) {
    Pixel* data = (Pixel*)image;
    Pixel* buffer = (Pixel*)malloc(64 * 4);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel prev, prev1, curr1;
    Pixel curr = {0, 0, 0, 255};

    un curr_index = 0;
    un img_size = desc->height * desc->width;
    bool qoi_op_run, qoi_op_index, qoi_op_diff, qoi_op_luma;
    char dg, dr_dg, db_dg;
    unchar chosen, res;

    unchar* output = (unchar*)malloc(img_size * 5);
    un out_len = 0;

    while (curr_index < img_size) {
        prev = curr;
        curr = data[curr_index];
        if (curr_index > 0) {
            buffer[hash(prev)] = prev;
        }

        qoi_op_run = cond_eq(prev, curr);
        qoi_op_index = cond_eq(curr, buffer[hash(curr)]) ? true : false;
        qoi_op_diff = cond_diff(curr, prev);
        qoi_op_luma = cond_luma(curr, prev);

        if (qoi_op_run) {
            chosen = QOI_OP_RUN;
            un run_len = 1;
            prev1 = curr;

            if (curr_index + 1 < img_size) {
                curr1 = data[curr_index + run_len];
            } else {
                output[out_len] = QOI_OP_RUN;
                out_len++;
                curr_index++;
                continue;
            }

            while (cond_eq(prev1, curr1) && run_len < 62 &&
                   curr_index + run_len + 1 < img_size) {
                run_len += 1;
                prev1 = curr1;
                curr1 = data[curr_index + run_len];
            }

            output[out_len] = QOI_OP_RUN + run_len - 1;
            out_len++;
            curr_index += run_len - 1;
        } else if (qoi_op_index) {
            chosen = QOI_OP_INDEX;
            output[out_len] = hash(curr);
            out_len++;
        } else if (qoi_op_diff) {
            chosen = QOI_OP_DIFF;
            res = QOI_OP_DIFF + ((curr.r + 2 - prev.r) << 4) +
                  ((curr.g + 2 - prev.g) << 2) + curr.b + 2 - prev.b;
            output[out_len] = res;
            out_len++;
        } else if (qoi_op_luma) {
            chosen = QOI_OP_LUMA;
            dg = curr.g - prev.g;
            dr_dg = curr.r - prev.r - dg;
            db_dg = curr.b - prev.b - dg;
            output[out_len] = QOI_OP_LUMA + dg + 32;
            output[out_len + 1] = ((dr_dg + 8) << 4) + db_dg + 8;
            out_len += 2;
        } else if (curr.a == prev.a) {
            chosen = QOI_OP_RGB;
            output[out_len] = QOI_OP_RGB;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            out_len += 4;
        } else {
            chosen = QOI_OP_RGBA;
            output[out_len] = QOI_OP_RGBA;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            output[out_len + 4] = curr.a;
            out_len += 5;
        }
        curr_index++;
    }
    output = realloc(output, out_len);

    free(buffer);
    return (Vector){output, out_len};
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

    Pixel* buffer = (Pixel*)malloc(256);
    if (buffer == NULL) {
        printf("malloc failed! \n");
        return (Vector){0, 0};
    }

    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel prev = {0, 0, 0, 255};

    un out_size = desc->width * desc->height * desc->channels;

    Pixel* output = (Pixel*)malloc(out_size);
    if (output == 0) {
        free(buffer);
        printf("malloc failed! \n");
        return (Vector){0, 0};
    }

    un out_len = 0;
    unchar byte1, byte2;
    un run;
    int diff_g;

    while (curr_index < image.len) {
        switch (decode_byte(data[curr_index])) {
            case 1:
                free(buffer);
                free(output);
                printf("Incorrect byte sequence!\n");
                return (Vector){0, 0};
            case QOI_OP_INDEX:
                prev = buffer[data[curr_index]];
                output[out_len] = prev;
                out_len++;
                break;
            case QOI_OP_DIFF:
                byte1 = data[curr_index];
                prev = (Pixel){.r = prev.r + ((byte1 >> 4) - 6),
                               .g = prev.g + ((byte1 >> 2) % 4 - 2),
                               .b = prev.b + (byte1 % 4 - 2),
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                break;
            case QOI_OP_LUMA:
                byte1 = data[curr_index];
                byte2 = data[curr_index + 1];
                diff_g = byte1 - QOI_OP_LUMA - 32;
                prev = (Pixel){.r = prev.r + diff_g + (byte2 >> 4) - 8,
                               .g = prev.g + diff_g,
                               .b = prev.b + diff_g + byte2 % 16 - 8,
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                curr_index++;
                break;
            case QOI_OP_RUN:
                run = data[curr_index] - QOI_OP_RUN + 1;
                for (un i = 0; i < run; i++) {
                    output[out_len + i] = prev;
                }
                out_len += run;
                break;
            case QOI_OP_RGB:
                prev = (Pixel){.r = data[curr_index + 1],
                               .g = data[curr_index + 2],
                               .b = data[curr_index + 3],
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                curr_index += 3;
                break;
            case QOI_OP_RGBA:
                prev = (Pixel){.r = data[curr_index + 1],
                               .g = data[curr_index + 2],
                               .b = data[curr_index + 3],
                               .a = data[curr_index + 4]};
                output[out_len] = prev;
                out_len++;
                curr_index += 4;
                break;
        }
        buffer[hash(prev)] = prev;
        curr_index++;
    }
    free((void*)buffer);
    return (Vector){output, out_len * 4};
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
    // printf("%lx\n", one);
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

    void* buff1 = malloc(4);
    un magic_s = fread(buff1, 1, 4, f);
    char* magic = (char*)buff1;

    if (strncmp(magic, "qoif", 4)) {
        printf("Not a .qoi file");
        return (Vector){0, 0};
    }
    free(buff1);

    un desc_s = fread(desc, 1, 10, f);
    un img_size = size - 22;
    void* image = malloc(img_size);
    un image_s = fread(image, img_size, 1, f);

    fclose(f);
    return (Vector){image, img_size};
}

int main() {
    printf("\n");

    qoi_desc desc = {0, 0, 0, 0};
    Vector r1 = read_qoi("assets/kodim23.qoi", &desc);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    const qoi_desc desc1 = desc;
    printf("%lu\n", r1.len);
    Vector r2 = qoi_decode(r1, &desc1);
    // free(r1.data);
    Vector r3 = qoi_encode(r2.data, &desc1);
    // free(r2.data);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    un r = write_qoi("assets/img.qoi", r3, &desc);
    // free(r3.data);

    printf("\n");
    return 0;
}
