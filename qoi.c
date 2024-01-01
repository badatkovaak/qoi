#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define cond_diff(p, c) (c.a == p.a) && (c.r - p.r + 2) <= 3 && (c.r - p.r + 2) >= 0 &&(c.g - p.g + 2) <= 3 && (c.g - p.g + 2) >= 0 && (c.b - p.b + 2) <= 3 && (c.b - p.b + 2) >= 0

#define cond_luma(p, c)                                                                                                \
    (c.a == p.a) && (c.g - p.g + 32) <= 63 && (c.g - p.g + 32) >= 0 && (c.r - p.r - c.g + p.g + 8) <= 15 && (c.r - p.r - c.g + p.g + 8) >= 0 && (c.b - p.b - c.g + p.g + 8) <= 15 && (c.b - p.b - c.g + p.g + 8) >= 0

#define cond_eq(p, c) (p.r == c.r) && (p.g == c.g) && (p.b == c.b) && (p.a == c.a)

typedef unsigned char unchar;
typedef uint64_t un;

typedef struct
{
    unchar *data;
    un len;
} vec;

typedef struct
{
    void *data;
    un len;
} data_w_len;

typedef struct
{
    uint32_t width;
    uint32_t height;
    unchar channels;
    unchar colorspace;
} qoi_desc;

typedef struct
{
    unchar r;
    unchar g;
    unchar b;
} pixel_rgb;

typedef struct
{
    unchar r;
    unchar g;
    unchar b;
    unchar a;
} pixel;

uint32_t switch_endianness(uint32_t value)
{
    return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) | ((value << 8) & 0xFF0000) | ((value << 24) & 0xFF000000);
}

pixel generate_pixel_rgba()
{
    pixel res = {
        rand() % 256,
        rand() % 256,
        rand() % 256,
        // rand() % 16 + 120,
        // rand() % 16 + 120,
        // rand() % 16 + 120,
        // rand() % 32 + 112,
        255,
    };
    return res;
}

pixel *generate_data_rgba(uint32_t width, uint32_t height)
{
    un size = width * height;
    pixel *data = (pixel *)malloc(size * 4);
    for (un i = 0; i < size; i++)
    {
        data[i] = generate_pixel_rgba();
    }
    return data;
}

void print_image(const pixel *d, un size)
{
    for (un i = 0; i < size; i++)
    {
        printf("r: %u, g: %u, b: %u, a: %u\n", d[i].r, d[i].g, d[i].b, d[i].a);
    }
}

char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return 0;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = (char *)malloc(size);

    fread(buffer, size, 1, f);
    fclose(f);

    return buffer;
}

vec read_file_bin(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        vec r = {0, 0};
        return r;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unchar *buffer = (unchar *)malloc(size);

    fread(buffer, size, 1, f);
    fclose(f);

    vec res = {buffer, size};
    return res;
}

un write_to_file(const char *path, char *data, size_t size)
{
    FILE *f = fopen(path, "w");
    if (f == 0)
        return 0;
    fwrite(data, size, 1, f);
    fclose(f);
    return 1;
}

void print_unchar(unchar *d, size_t len)
{
    for (un i = 0; i < len; i++)
    {
        printf("%x ", d[i]);
    }
    printf("\n");
}

data_w_len qoi_encode(const void *image, const qoi_desc *desc)
{
    pixel *data = (pixel *)image;
    // pixel *running = (pixel *)malloc(64 * 4);

    pixel prev = {0, 0, 0, 255};
    pixel curr = {0, 0, 0, 255};
    un curr_index = 0;
    un img_size = desc->height * desc->width;
    bool cond1, cond2, cond3;
    unchar *output = (unchar *)malloc(img_size * 5);
    un out_len = 0;

    while (curr_index < img_size)
    {
        prev = curr;
        curr = data[curr_index];

        cond1 = cond_eq(prev, curr);
        cond2 = cond_diff(prev, curr);
        cond3 = cond_luma(prev, curr);

        if (cond1)
        {
            un run_len = 1;
            pixel prev1 = curr;
            pixel curr1 = data[curr_index + run_len];

            while (cond_eq(prev1, curr1) && run_len < 63)
            {
                run_len += 1;
                prev1 = curr1;
                curr1 = data[curr_index + run_len];
            }
            output[out_len] = 0xc0 + run_len - 1;
            out_len++;
            curr_index += run_len - 1;
            printf("1: %x \n", 0xc0 + run_len - 1);
        }
        else if (cond2)
        {
            printf("2: r: %u %u, g: %u %u , b: %u %u \n", curr.r,prev.r, curr.g,prev.g,curr.b,prev.b);
            unchar res =0x40 + (curr.r - prev.r + 2) * 16 + (curr.g - prev.g + 2) * 4 + curr.b - prev.b + 2;
            output[out_len] = res;
            out_len++;
            printf("2: %x \n", res);
        }
        else if (cond3)
        {
            char dg = curr.g - prev.g;
            char dr_dg = curr.r - prev.r - dg;
            char db_dg = curr.b - prev.b - dg;
            output[out_len] = 0x80 + dg + 32;
            output[out_len + 1] = (dr_dg + 8) * 16 + db_dg + 8;
            out_len += 2;
            printf("3: %x  %x\n", 0x80 + dg + 32, (dr_dg + 8)*16 + db_dg + 8);
        }
        else if (curr.a == prev.a)
        {
            output[out_len] = 0xfe;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            out_len += 4;
            printf("4: 0xfe %x  %x  %x \n", curr.r,curr.g, curr.b);
        }
        else
        {
            output[out_len] = 0xff;
            output[out_len + 1] = curr.r;
            output[out_len + 2] = curr.g;
            output[out_len + 3] = curr.b;
            output[out_len + 4] = curr.a;
            out_len += 5;
            printf("5: 0xff %x  %x  %x  %x  \n", curr.r, curr.g, curr.b, curr.a);
        }
        curr_index++;
    }

    data_w_len res = {(void *)output, out_len};
    return res;
}

// data_w_len qoi_encode_lazy(const void *image, const qoi_desc *desc)
// {
//     pixel *data = (pixel *)image;
//     un img_size = desc->width * desc->height;
//     unchar *output = (unchar *)malloc(img_size * 5);
//     un out_len = 0;
// }

un write_qoi(const char *path, const void *data, const qoi_desc *desc, un out_len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        printf("Error!\n");
        return 0;
    }

    const char *magic = "qoif";
    un mg = fwrite(magic, 1, 4, f);
    un header = fwrite(desc, 1, 10, f);
    un res = fwrite(data, 1, out_len, f);
    uint64_t one = 1ULL << (8 * 7);
    printf("%x\n", one);
    un on = fwrite(&one, 1, 8, f);
    return mg + header + res + on;
}

int main()
{
    // srand(time(0));

    // vec d = read_file_bin("assets/testcard_rgba.qoi");
    // vec d = read_file_bin("assets/img.qoi");
    // print_unchar(d.data, d.len);

    const uint32_t width = 16;
    const uint32_t height = 16;
    const pixel *image = generate_data_rgba(width, height);
    print_image(image, height * width);
    const qoi_desc desc = {width, height, 4, 0};
    const data_w_len d = qoi_encode(image, &desc);
    const qoi_desc desc1 = {switch_endianness(width), switch_endianness(height), 4, 1};
    un r = write_qoi("assets/img.qoi", d.data, &desc1, d.len);
    printf("%lu\n", r);

    return 0;
}
