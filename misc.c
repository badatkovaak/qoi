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
//
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
