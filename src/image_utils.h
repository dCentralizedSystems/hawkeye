
#ifndef JPEG_UTILS_H
#define JPEG_UTILS_H

#include <stdbool.h>

#include "color_detect.h"

size_t
compress_yuyv_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char *src, size_t src_size, unsigned int width,
                      unsigned int height, int quality, bool enable_stripe_detect, bool b_write_detect_image);
size_t compress_z16_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality, int mm_scale);

#endif
