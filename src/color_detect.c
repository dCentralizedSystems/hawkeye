//
// Created by rsnook on 5/28/20.
//
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "color_detect.h"

// internal type to hold detections and associated data
typedef struct {
    blob_t blobs[COLOR_DETECT_NUM_BLOBS_MAX];
    size_t num_blobs;
} detections_t;

// the detections data structure
static detections_t detections;

bool calcNorms(detect_color_t* p_detect_color) {

    // avoid divide by zeroes
    if (p_detect_color->red <= 0.0f) {
        p_detect_color->red = 0.001f;
    }

    if (p_detect_color->green <= 0.0f) {
        p_detect_color->green = 0.001f;
    }

    if (p_detect_color->blue <= 0.0f) {
        p_detect_color->blue = 0.001f;
    }

    // calculate and store detect color parameters
    p_detect_color->norm = sqrt((p_detect_color->red * p_detect_color->red) + (p_detect_color->green * p_detect_color->green) + (p_detect_color->blue * p_detect_color->blue));

    p_detect_color->red_norm = p_detect_color->red / p_detect_color->norm;
    p_detect_color->green_norm = p_detect_color->green / p_detect_color->norm;
    p_detect_color->blue_norm = p_detect_color->blue / p_detect_color->norm;

    p_detect_color->gr_norm = p_detect_color->green_norm / p_detect_color->red_norm;
    p_detect_color->gb_norm = p_detect_color->green_norm / p_detect_color->blue_norm;
    p_detect_color->rb_norm = p_detect_color->red_norm / p_detect_color->blue_norm;

    return true;
}

bool rgb_match(detect_color_t *p_detect_color, uint8_t red, uint8_t green, uint8_t blue, float tolerance) {

    /* Min / max ratio skew */
    const float norm_min = 1.0f - tolerance;
    const float norm_max = 1.0f + tolerance;

    detect_color_t in_pix_color;

    in_pix_color.red = red;
    in_pix_color.blue = blue;
    in_pix_color.green = green;

    if (!calcNorms(&in_pix_color)) {
        return false;
    }

    if (in_pix_color.gr_norm > p_detect_color->gr_norm * norm_min && in_pix_color.gr_norm < p_detect_color->gr_norm * norm_max) {
        if (in_pix_color.gb_norm > p_detect_color->gb_norm * norm_min && in_pix_color.gb_norm < p_detect_color->gb_norm * norm_max) {
            if (in_pix_color.rb_norm > p_detect_color->rb_norm * norm_min && in_pix_color.rb_norm < p_detect_color->rb_norm * norm_max) {
                return true;
            }
        }
    }

    return false;
}

static void clear_blobs(void) {
    detections.num_blobs = 0;

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        memset(&detections.blobs[i], 0, sizeof(blob_t));
    }
}

static int detect_blobs(uint8_t* p_image, int width, int height, uint8_t detect_color) {

    uint8_t *p_curr = p_image;

    if (p_curr == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    // Iterate over input image
    for (size_t h=0; h < height; ++h) {
        for (size_t w=0; w < width; ++w) {
            // look for pixel value
            if (*p_curr++ == detect_color) {

            }
        }
    }
    return 0;
}

// assumes pixels packed RGBRGBRGB...3 bytes per pixel
void rgb_color_detection(uint8_t *p_pix, uint32_t pixSize, int width, int height, detect_color_t *p_detect_color, float detect_tolerance) {

    size_t detect_image_size = width * height;

    if (pixSize != width * height * 3) {
        printf("%s: invalid pixel buffer size (%u != %d * %d)\n", __func__, pixSize, width, height);
        return;
    }

    uint8_t *p_detect_image_start = (uint8_t*)malloc(detect_image_size);
    uint8_t *p_detect_image = p_detect_image_start;

    // iterate over input image buffer and write 0 if specified color not detected, 1 if detected
    uint8_t *p_input = p_pix;

    for (uint32_t h=0; h < height; ++h) {
        for (uint32_t w=0; w < width; ++w) {
            // decode one pixel
            uint8_t blue = *p_input++;
            uint8_t green = *p_input++;
            uint8_t red = *p_input++;

            if (rgb_match(p_detect_color, red, green, blue, detect_tolerance)) {
                *p_detect_image++ = 128;
            } else {
                *p_detect_image++ = 0;
            }
        }
    }

#ifdef WRITE_DETECT_FILE
    FILE* p_file = fopen(p_color_detect_file_name, "w+");

    if (p_file == NULL) {
        panic("Can't write output image file.");
        free(p_detect_image_start);
        return;
    }

    // write image
    bmWriteBitmap(p_file, width, height, 1, p_detect_image_start, detect_image_size);

    fflush(p_file);
    fclose(p_file);

    /* Now that write is complete, rename the file */
    rename(p_color_detect_file_name, p_color_detect_file_rename);
#endif /* #ifdef WRITE_DETECT_FILE */

    free(p_detect_image_start);
    p_detect_image_start = NULL;
}



