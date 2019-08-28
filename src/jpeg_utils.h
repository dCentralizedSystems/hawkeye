
#ifndef JPEG_UTILS_H
#define JPEG_UTILS_H

size_t compress_yuyv_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality);
size_t compress_z16_to_jpeg_and_depth_profile(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality);

#endif
