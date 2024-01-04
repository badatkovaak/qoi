#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define cond_diff(p, c)                                             \
    (c.a == p.a) && (c.r - p.r + 2) <= 3 && (c.r - p.r + 2) >= 0 && \
        (c.g - p.g + 2) <= 3 && (c.g - p.g + 2) >= 0 &&             \
        (c.b - p.b + 2) <= 3 && (c.b - p.b + 2) >= 0

#define cond_luma(p, c)                                                \
    (c.a == p.a) && (c.g - p.g + 32) <= 63 && (c.g - p.g + 32) >= 0 && \
        (c.r - p.r - c.g + p.g + 8) <= 15 &&                           \
        (c.r - p.r - c.g + p.g + 8) >= 0 &&                            \
        (c.b - p.b - c.g + p.g + 8) <= 15 && (c.b - p.b - c.g + p.g + 8) >= 0

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

Pixel generate_pixel_rgba(unchar mode) {
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
    Pixel* data = (Pixel*)malloc(size * 4);
    for (un i = 0; i < size; i++) {
        data[i] = generate_pixel_rgba(mode);
    }
    return data;
}

void print_image(const Pixel* d, un size) {
    for (un i = 0; i < size; i++) {
        printf("r: %u, g: %u, b: %u, a: %u\n", d[i].r, d[i].g, d[i].b, d[i].a);
    }
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

un write_image(const char* path, Vector image) {
    FILE* f = fopen(path, "w");
    if (f == 0) return 0;
    un res = fwrite(image.data, 1, image.len, f);
    fclose(f);
    return res;
}

void print_vec(Vector v) {
    for (un i = 0; i < v.len; i++) {
        printf("%x ", ((unchar*)v.data)[i]);
    }
    printf("\n");
}

// int cond_buf(Pixel c, Pixel* buffer, size_t len) {
//     // for (un i = 0; i < len; i++) {
//     //     if (cond_eq(c, buffer[i])) return i;
//     // }
//     return -1;
// }

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

Vector qoi_encode(const void* image, const qoi_desc* desc) {
    Pixel* data = (Pixel*)image;
    Pixel* buffer = (Pixel*)malloc(64 * 4);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel prev;
    Pixel curr = {0, 0, 0, 255};

    un curr_index = 0;
    un img_size = desc->height * desc->width;
    bool qoi_op_run, qoi_op_index, qoi_op_diff, qoi_op_luma;
    // int j;
    char dg, dr_dg, db_dg;
    unchar chosen, res;

    unchar* output = (unchar*)malloc(img_size * 5);
    un out_len = 0;

    while (curr_index < img_size) {
        // printf("%u", curr_index);
        prev = curr;
        curr = data[curr_index];
        // if (!curr_index) {
        buffer[hash(prev)] = prev;
        // }

        qoi_op_run = cond_eq(prev, curr);
        // j = cond_buf(curr, buffer, 64);

        qoi_op_index = cond_eq(curr, buffer[hash(curr)]) ? true : false;
        qoi_op_diff = cond_diff(prev, curr);
        qoi_op_luma = cond_luma(prev, curr);

        if (qoi_op_run) {
            chosen = QOI_OP_RUN;
            un run_len = 1;
            Pixel prev1 = curr;
            Pixel curr1;

            if (curr_index + 1 < img_size) {
                curr1 = data[curr_index + run_len];
            } else {
                output[out_len] = QOI_OP_RUN;
                out_len++;
                curr_index++;
                continue;
            }

            while (cond_eq(prev1, curr1) && run_len < 63 &&
                   curr_index + run_len + 1 < img_size) {
                run_len += 1;
                prev1 = curr1;
                curr1 = data[curr_index + run_len];
            }

            output[out_len] = QOI_OP_RUN + run_len - 1;
            out_len++;
            curr_index += run_len - 1;
            // printf("1: %lx \n", 0xc0 + run_len - 1);
        } else if (qoi_op_index) {
            chosen = QOI_OP_INDEX;
            output[out_len] = hash(curr);
            if (curr_index < 1000) {
                // printf("%06lu %02d %08x %08x\n", curr_index, j, buffer[j],
                //        curr);
                // print_buffer(buffer, 64);
            }
            out_len++;
            // printf("2: %x \n", j);
        } else if (qoi_op_diff) {
            chosen = QOI_OP_DIFF;
            // printf("3: r: %u %u, g: %u %u , b: %u %u \n", curr.r, prev.r,
            // curr.g, prev.g, curr.b, prev.b);
            res = QOI_OP_DIFF + ((curr.r - prev.r + 2) << 4) +
                  ((curr.g - prev.g + 2) << 2) + curr.b - prev.b + 2;
            output[out_len] = res;
            out_len++;
            // printf("3: %x \n", res);
        } else if (qoi_op_luma) {
            chosen = QOI_OP_LUMA;
            dg = curr.g - prev.g;
            dr_dg = curr.r - prev.r - dg;
            db_dg = curr.b - prev.b - dg;
            output[out_len] = QOI_OP_LUMA + dg + 32;
            output[out_len + 1] = ((dr_dg + 8) << 4) + db_dg + 8;
            // printf(
            //     "%06lu curr - r: %02x, g: %02x, b: %02x, a: %02x\n       prev
            //     "
            //     "-"
            //     " r: %02x, g: %02x, b: %02x, a: %02x\n       %02x  %02x  %02x
            //     "
            //     "%02x  %02x\n",
            //     curr_index, curr.r, curr.g, curr.b, curr.a, prev.r, prev.g,
            //     prev.b, prev.a, dg + 32, dr_dg + 8, db_dg + 8,
            //     output[out_len], output[out_len + 1]);
            out_len += 2;
            // printf("4: %x  %x\n", 0x80 + dg + 32, (dr_dg + 8) * 16 + db_dg +
            // 8);
        } else if (curr.a == prev.a) {
            chosen = QOI_OP_RGB;
            output[out_len] = QOI_OP_RGB;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            out_len += 4;
            // printf("5: 0xfe %x  %x  %x \n", curr.r, curr.g, curr.b);
        } else {
            chosen = QOI_OP_RGBA;
            output[out_len] = QOI_OP_RGBA;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            output[out_len + 4] = curr.a;
            out_len += 5;
            // printf("6: 0xff %x  %x  %x  %x  \n", curr.r, curr.g, curr.b,
            // curr.a);
        }
        // printf("%05lu %02x %08x %08x\n", curr_index, chosen, prev, curr);
        curr_index++;
    }
    output = realloc(output, out_len);

    free(buffer);
    // data_w_len res = {(void *)output, out_len};
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

    // Pixel curr = {0,0,0,255};
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
    printf("%lu\n", out_size);

    while (curr_index < image.len) {
        // printf("%lu %x %lu\n", curr_index, data[curr_index], out_len);
        switch (decode_byte(data[curr_index])) {
            case 1:
                free(buffer);
                free(output);
                printf("Incorrect byte sequence!\n");
                return (Vector){0, 0};
            case QOI_OP_INDEX:
                // printf("case 1\n");
                prev = buffer[data[curr_index]];
                output[out_len] = prev;
                out_len++;
                break;
            case QOI_OP_DIFF:
                // printf("case 2\n");
                byte1 = data[curr_index];
                prev = (Pixel){.r = prev.r + ((byte1 >> 4) - 6),
                               .g = prev.g + ((byte1 >> 2) % 4 - 2),
                               .b = prev.b + (byte1 % 4 - 2),
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                break;
            case QOI_OP_LUMA:
                // printf("case 3\n");
                byte1 = data[curr_index];
                byte2 = data[curr_index + 1];
                diff_g = byte1 - QOI_OP_LUMA - 32;
                // printf("%x %x %d %lu\n", byte1, byte2, diff_g, curr_index);
                prev = (Pixel){.r = prev.r + diff_g + (byte2 >> 4) - 8,
                               .g = prev.g + diff_g,
                               .b = prev.b + diff_g + byte2 % 16 - 8,
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                curr_index++;
                break;
            case QOI_OP_RUN:
                // printf("case 4: ");
                run = data[curr_index] - QOI_OP_RUN + 1;
                // printf("%x %lu %lu\n", data[curr_index], curr_index, run);
                for (un i = 0; i < run; i++) {
                    // printf("hi there %lu %lu\n", out_len + i, out_size);
                    output[out_len + i] = prev;
                }
                out_len += run;
                break;
            case QOI_OP_RGB:
                // printf("case 5\n");
                prev = (Pixel){.r = data[curr_index + 1],
                               .g = data[curr_index + 2],
                               .b = data[curr_index + 3],
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                curr_index += 3;
                break;
            case QOI_OP_RGBA:
                // printf("case 6\n");
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
    printf("%lu %lu\n", out_len, curr_index);
    // printf("im\n");
    free((void*)buffer);
    // printf("invalid\n");
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

Vector decode_to_full_rep(Vector image, const qoi_desc* desc) {
    un curr_index = 0;
    unchar* data = (unchar*)image.data;
    un out_size = desc->width * desc->height * 5;

    full_rep* output = (full_rep*)malloc(out_size);
    if (output == 0) {
        printf("malloc failed! \n");
        return (Vector){0, 0};
    }

    un out_len = 0;
    unchar byte1, byte2;
    un run;
    int diff_g;
    printf("%lu\n", out_size);

    while (curr_index < image.len) {
        switch (decode_byte(data[curr_index])) {
            case 1:
                free(output);
                printf("Incorrect byte sequence!\n");
                return (Vector){0, 0};
            case QOI_OP_INDEX:
                output[out_len] = (full_rep){
                    .op = data[curr_index], .r = 0, .g = 0, .b = 0, .a = 0};
                out_len++;
                break;
            case QOI_OP_DIFF:
                output[out_len] = (full_rep){
                    .op = data[curr_index], .r = 0, .g = 0, .b = 0, .a = 0};
                out_len++;
                break;
            case QOI_OP_LUMA:
                output[out_len] = (full_rep){.op = data[curr_index],
                                             .r = data[curr_index + 1],
                                             .g = 0,
                                             .b = 0,
                                             .a = 0};
                out_len++;
                curr_index++;
                break;
            case QOI_OP_RUN:
                run = data[curr_index] - QOI_OP_RUN + 1;
                output[out_len] = (full_rep){
                    .op = data[curr_index], .r = 0, .g = 0, .b = 0, .a = 0};
                for (un i = 1; i < run; i++) {
                    output[out_len + i] =
                        (full_rep){.op = 0, .r = 0, .g = 0, .b = 0, .a = 0};
                }
                out_len += run;
                break;
            case QOI_OP_RGB:
                output[out_len] = (full_rep){.op = data[curr_index],
                                             .r = data[curr_index + 1],
                                             .g = data[curr_index + 2],
                                             .b = data[curr_index + 3],
                                             .a = 0};
                out_len++;
                curr_index += 3;
                break;
            case QOI_OP_RGBA:
                output[out_len] = (full_rep){.op = data[curr_index],
                                             .r = data[curr_index + 1],
                                             .g = data[curr_index + 2],
                                             .b = data[curr_index + 3],
                                             .a = data[curr_index + 4]};
                out_len++;
                curr_index += 4;
                break;
        }
        curr_index++;
    }
    return (Vector){output, out_len};
}

Vector encode_to_full_rep(const void* image, const qoi_desc* desc) {
    Pixel* data = (Pixel*)image;
    Pixel* buffer = (Pixel*)malloc(64 * 4);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    Pixel prev = {0, 0, 0, 255};
    Pixel curr = {0, 0, 0, 255};

    un curr_index = 0;
    un img_size = desc->height * desc->width;
    bool cond1, cond2, cond3, cond4;
    int j;

    unchar* output = (unchar*)malloc(desc->width * desc->height * 5);
    un out_len = 0;

    while (curr_index < img_size) {
        // printf("%u", curr_index);
        prev = curr;
        curr = data[curr_index];
        buffer[hash(prev)] = prev;

        cond1 = cond_eq(prev, curr);
        j = -1;
        // j = cond_buf(curr, buffer, 64);
        cond2 = j < 0 ? false : true;
        cond3 = cond_diff(prev, curr);
        cond4 = cond_luma(prev, curr);

        if (cond1) {
            un run_len = 1;
            Pixel prev1 = curr;
            Pixel curr1;

            if (curr_index + 1 < img_size) {
                curr1 = data[curr_index + run_len];
            } else {
                output[out_len] = QOI_OP_RUN + run_len - 1;
                out_len++;
                curr_index++;
                continue;
            }

            while (cond_eq(prev1, curr1) && run_len < 63 &&
                   curr_index + run_len + 1 < img_size) {
                run_len += 1;
                prev1 = curr1;
                curr1 = data[curr_index + run_len];
            }

            output[out_len] = QOI_OP_RUN + run_len - 1;
            out_len++;
            curr_index += run_len - 1;
            // printf("1: %lx \n", 0xc0 + run_len - 1);
        } else if (cond2) {
            output[out_len] = j;
            out_len++;
            // printf("2: %x \n", j);
        } else if (cond3) {
            // printf("3: r: %u %u, g: %u %u , b: %u %u \n", curr.r, prev.r,
            // curr.g, prev.g, curr.b, prev.b);
            unchar res = QOI_OP_DIFF + (curr.r - prev.r + 2) * 16 +
                         (curr.g - prev.g + 2) * 4 + curr.b - prev.b + 2;
            output[out_len] = res;
            out_len++;
            // printf("3: %x \n", res);
        } else if (cond4) {
            char dg = curr.g - prev.g;
            char dr_dg = curr.r - prev.r - dg;
            char db_dg = curr.b - prev.b - dg;
            output[out_len] = QOI_OP_LUMA + dg + 32;
            output[out_len + 1] = (dr_dg + 8) * 16 + db_dg + 8;
            out_len += 2;
            // printf("4: %x  %x\n", 0x80 + dg + 32, (dr_dg + 8) * 16 + db_dg +
            // 8);
        } else if (curr.a == prev.a) {
            output[out_len] = QOI_OP_RGB;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            out_len += 4;
            // printf("5: 0xfe %x  %x  %x \n", curr.r, curr.g, curr.b);
        } else {
            output[out_len] = QOI_OP_RGBA;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            output[out_len + 4] = curr.a;
            out_len += 5;
            // printf("6: 0xff %x  %x  %x  %x  \n", curr.r, curr.g, curr.b,
            // curr.a);
        }
        curr_index++;
    }
    output = realloc(output, out_len);

    free(buffer);
    // data_w_len res = {(void *)output, out_len};
    return (Vector){output, out_len};
}

void print_diff(Vector v1, Vector v2) {
    printf("\n");
    for (un i = 0; i < v1.len || i < v2.len; i++) {
        if (i < v1.len && i < v2.len) {
            printf("%x  --  %x\n", ((unchar*)v1.data)[i],
                   ((unchar*)v2.data)[i]);
        } else {
            if (i < v1.len) {
                printf("%x  --  _\n", ((unchar*)v1.data)[i]);
            } else {
                printf("_  --  %x\n", ((unchar*)v2.data)[i]);
            }
        }
    }
    printf("\n\n");
}

void full_diff(Pixel* decoded, Vector rep) {
    printf("\n");
    Pixel c;
    full_rep r;
    for (un i = 0; i < rep.len; i++) {
        c = decoded[i];
        r = ((full_rep*)rep.data)[i];
        printf("%02x %02x %02x %02x ----- %02x %02x %02x %02x %02x\n\n", c.r,
               c.g, c.b, c.a, r.op, r.r, r.g, r.b, r.a);
    }
    printf("\n");
}

// void print_3_diff(Vector v1, Vector v2, Vector v3){
//
//     for(un i = 0; i < v1.len || i < v2.len || i < v3.len;i++){
//         if (i < v1.len){
//             printf("%02x %02x %02x %02x   ", v1.data);
//         }
//         if (i < v2.len){
//
//         }
//     }
// }

int main() {
    printf("\n");
    // srand(time(0));

    // Vector v1 = read_file_bin("assets/rand1.raw");
    // // print_vec(v1);
    // qoi_desc desc1 = {0, 0, 0, 0};
    // Vector v2 = read_qoi("assets/rand1.qoi", &desc1);
    // desc1.height = switch_endianness(desc1.height);
    // desc1.width = switch_endianness(desc1.width);
    // printf("%x %x %x %x \n", desc1.width, desc1.height, desc1.channels,
    //        desc1.colorspace);
    // print_vec(v2);
    // Vector v3 = qoi_decode(v2, &desc1);
    // Vector v4 = decode_to_full_rep(v2, &desc1);
    // Vector v5 = qoi_encode(v1.data, &desc1);
    // // print_diff(v1, v3);
    // full_diff(v3.data, v4);
    // printf("%lu %lu %lu\n", v1.len, v2.len, v3.len);
    // printf("%lu\n", sizeof(Pixel));
    // free(v1.data);
    // free(v2.data);
    // free(v3.data);
    // free(v4.data);

    // vec d = read_file_bin("assets/testcard_rgba.qoi");
    // Vector d = read_file_bin("assets/img.qoi");
    // print_vec(d);
    // printf("\n\n\n\n\n\n\n\n");

    qoi_desc desc = {0, 0, 0, 0};

    Vector r1 = read_qoi("assets/testcard.qoi", &desc);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    const qoi_desc desc1 = desc;
    // print_unchar(r1.data, r1.len);

    // printf("\n\n\n\n");
    printf("%lu\n", r1.len);
    Vector r2 = qoi_decode(r1, &desc1);
    // un k = write_image("assets/test1.bin", r2);
    free(r1.data);
    // print_vec((Vector){r2.data, 10000});
    // printf("%x %x %x %x \n", desc.width, desc.height, desc.channels,
    // desc.colorspace);
    // printf("\n\n\n\n");
    Vector r3 = qoi_encode(r2.data, &desc1);
    free(r2.data);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    // const qoi_desc desc1 = desc;
    // print_vec((Vector){r3.data, 10000});
    un r = write_qoi("assets/img.qoi", r3, &desc);
    free(r3.data);

    printf("\n");
    return 0;
}
