#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

Pixel generate_pixel_rgba() {
    Pixel res = {
        // rand() % 256,
        // rand() % 256,
        // rand() % 256,
        rand() % 16 + 120,
        rand() % 16 + 120,
        rand() % 16 + 120,
        // rand() % 32 + 112,
        255,
    };
    return res;
}

Pixel* generate_data_rgba(uint32_t width, uint32_t height) {
    un size = width * height;
    Pixel* data = (Pixel*)malloc(size * 4);
    for (un i = 0; i < size; i++) {
        data[i] = generate_pixel_rgba();
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

un write_image(const char* path, void* image, size_t len) {
    FILE* f = fopen(path, "w");
    if (f == 0) return 0;
    un res = fwrite(image, len, 1, f);
    fclose(f);
    return res;
}

void print_unchar(unchar* d, size_t len) {
    for (un i = 0; i < len; i++) {
        printf("%x ", d[i]);
    }
    printf("\n");
}

int cond_buf(Pixel c, Pixel* buffer, size_t len) {
    for (un i = 0; i < len; i++) {
        if (cond_eq(c, buffer[i])) return i;
    }
    return -1;
}

Vector qoi_encode(const void* image, const qoi_desc* desc) {
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
        j = cond_buf(curr, buffer, 64);
        cond2 = j < 0 ? false : true;
        cond3 = cond_diff(prev, curr);
        cond4 = cond_luma(prev, curr);

        if (cond1) {
            un run_len = 1;
            Pixel prev1 = curr;
            Pixel curr1 = data[curr_index + run_len];

            while (cond_eq(prev1, curr1) && run_len < 63) {
                run_len += 1;
                prev1 = curr1;
                curr1 = data[curr_index + run_len];
            }

            output[out_len] = 0xc0 + run_len - 1;
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
            unchar res = 0x40 + (curr.r - prev.r + 2) * 16 +
                         (curr.g - prev.g + 2) * 4 + curr.b - prev.b + 2;
            output[out_len] = res;
            out_len++;
            // printf("3: %x \n", res);
        } else if (cond4) {
            char dg = curr.g - prev.g;
            char dr_dg = curr.r - prev.r - dg;
            char db_dg = curr.b - prev.b - dg;
            output[out_len] = 0x80 + dg + 32;
            output[out_len + 1] = (dr_dg + 8) * 16 + db_dg + 8;
            out_len += 2;
            // printf("4: %x  %x\n", 0x80 + dg + 32, (dr_dg + 8) * 16 + db_dg +
            // 8);
        } else if (curr.a == prev.a) {
            output[out_len] = 0xfe;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            out_len += 4;
            // printf("5: 0xfe %x  %x  %x \n", curr.r, curr.g, curr.b);
        } else {
            output[out_len] = 0xff;
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

    // data_w_len res = {(void *)output, out_len};
    return (Vector){output, out_len};
}

un decode_byte(unchar b) {
    if (b == 0xfe) {
        return 5;
    } else if (b == 0xff) {
        return 6;
    } else if (b <= 63) {
        return 1;
    } else if ((b >> 6) == 1) {
        return 2;
    } else if ((b >> 6) == 2) {
        return 3;
    } else if ((b >> 6) == 3) {
        return 4;
    }
    return 0;
}

Vector qoi_decode(const void* image, size_t length, const qoi_desc* desc) {
    un curr_index = 0;
    unchar* data = (unchar*)image;

    Pixel* buffer = (Pixel*)malloc(256);
    for (un i = 0; i < 64; i++) {
        buffer[i] = (Pixel){.r = 0, .g = 0, .b = 0, .a = 0};
    }

    // Pixel curr = {0,0,0,255};
    Pixel prev = {0, 0, 0, 255};

    Pixel* output = (Pixel*)malloc(desc->width * desc->height * desc->channels);
    un out_len = 0;
    unchar byte1, byte2;
    int buff_index;
    int diff_g;

    while (curr_index < length) {
        switch (decode_byte(data[curr_index])) {
            case 0:
                printf("Incorrect byte sequence!\n");
                return (Vector){0, 0};
            case 1:

                // buff_index = cond_buf(prev, buffer, 64);
                // if (buff_index != -1) {
                output[out_len] = buffer[data[curr_index]];
                //     prev = buffer[buff_index];
                out_len++;
                // } else {
                //     printf("");
                //     printf("No such pixel in buffer\n");
                //     return (Vector){0, 0};
                // }
                break;

            case 2:
                byte1 = data[curr_index];
                prev = (Pixel){.r = prev.r + ((byte1 >> 4) - 6),
                               .g = prev.g + ((byte1 >> 2) % 4 - 2),
                               .b = prev.b + (byte1 % 4 - 2),
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                break;
            case 3:
                byte1 = data[curr_index];
                byte2 = data[curr_index + 1];
                diff_g = prev.g + byte1 & 63 - 32;
                prev = (Pixel){.r = prev.r + diff_g + (byte2 >> 4) - 8,
                               .g = diff_g,
                               .b = prev.b + diff_g + byte2 % 16 - 8,
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                curr_index++;
                break;
            case 4:
                for (un i = 0; i < data[curr_index] + 1; i++) {
                    output[out_len + i] = prev;
                }
                out_len += data[curr_index];
                break;
            case 5:
                prev = (Pixel){.r = data[curr_index + 1],
                               .g = data[curr_index + 2],
                               .b = data[curr_index + 3],
                               .a = prev.a};
                output[out_len] = prev;
                out_len++;
                break;
            case 6:
                prev = (Pixel){.r = data[curr_index + 1],
                               .g = data[curr_index + 2],
                               .b = data[curr_index + 3],
                               .a = data[curr_index + 4]};
                output[out_len] = prev;
                out_len++;
                break;
        }
        buffer[hash(prev)] = prev;
        curr_index++;
    }
    return (Vector){output, out_len};
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
    printf("%lx\n", one);
    un on = fwrite(&one, 1, 8, f);
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
    un magic_s = fread(buff1, 4, 1, f);
    un desc_s = fread(desc, 10, 1, f);
    un img_size = size - 22;
    void* image = malloc(img_size);
    un image_s = fread(image, img_size, 1, f);

    fclose(f);
    return (Vector){image, img_size};
}

int main() {
    // srand(time(0));

    // vec d = read_file_bin("assets/testcard_rgba.qoi");
    Vector d = read_file_bin("assets/testcard.qoi");
    print_unchar(d.data, d.len);
    // free((void*)d.data);
    printf("\n\n\n\n");

    qoi_desc desc = {0, 0, 0, 0};

    Vector r1 = read_qoi("assets/testcard.qoi", &desc);
    desc.height = switch_endianness(desc.height);
    desc.width = switch_endianness(desc.width);
    print_unchar(r1.data, r1.len);

    printf("\n\n\n\n");
    const qoi_desc desc1 = desc;
    Vector r2 = qoi_decode(r1.data, r2.len, &desc1);
    print_unchar(r2.data, r2.len);
    printf("%x %x %x %x \n", desc.width, desc.height, desc.channels,
           desc.colorspace);
    printf("\n\n\n\n");
    Vector r3 = qoi_encode(r2.data, &desc1);
    print_unchar(r3.data, r3.len);
    un r = write_qoi("assets/img.qoi", r3, &desc1);

    // const uint32_t width = 16;
    // const uint32_t height = 16;
    //
    // const Pixel* image = generate_data_rgba(width, height);
    // // print_image(image, height * width);
    //
    // const qoi_desc desc = {width, height, e4, 0};
    // const void* data = malloc(width * height * 5);
    // const un len = qoi_encode(image, &desc, data);
    // // free((void*)image);
    //
    // const qoi_desc desc1 = {switch_endianness(width),
    // switch_endianness(height),
    //                         4, 1};
    // un r = write_qoi("assets/img.qoi", data, &desc1, len);
    // printf("%lu\n", r);
    // free((void*)d.data);

    // return 0;
}
