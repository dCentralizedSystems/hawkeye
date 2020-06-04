//
// Created by rsnook on 5/28/20.
//
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "color_detect.h"

#define MIN_HORIZ_PIXELS_FOR_FEATURE_LINE   (3)
#define MIN_BLOB_PIXELS                     (50)

#define WHITE_COLOR_TABLE_INDEX             (1)
#define BLACK_COLOR_TABLE_INDEX             (0)
#define GREEN_COLOR_TABLE_INDEX             (2)
#define PURPLE_COLOR_TABLE_INDEX            (3)

#define DETECT_COLOR_TABLE_INDEX            (PURPLE_COLOR_TABLE_INDEX)
#define NO_DETECT_COLOR_TABLE_INDEX         (BLACK_COLOR_TABLE_INDEX)
#define DRAW_BLOB_COLOR_TABLE_INDEX         (GREEN_COLOR_TABLE_INDEX)

// Filenames for output color-detect image
static const char* color_detect_file_name = "color-detect-image.bmp~";
static const char* color_detect_file_rename = "color-detect-image.bmp";

// internal type to hold detections and associated data
typedef struct {
    blob_t blobs[COLOR_DETECT_NUM_BLOBS_MAX];
    size_t num_blobs;
} detections_t;

static rgbColorTableEntry colorDetectColorTable[256] = { 0 };

void colorDetectInit(void) {
    // build color-detect color-table
    for (uint32_t i=0; i < 256; ++i) {
        colorDetectColorTable[i].blue = i;
        colorDetectColorTable[i].green = i;
        colorDetectColorTable[i].red = i;
        colorDetectColorTable[1].reserved = 0;
    }

    // store special values
    colorDetectColorTable[BLACK_COLOR_TABLE_INDEX].blue = 0;
    colorDetectColorTable[BLACK_COLOR_TABLE_INDEX].green = 0;
    colorDetectColorTable[BLACK_COLOR_TABLE_INDEX].red = 0;
    colorDetectColorTable[BLACK_COLOR_TABLE_INDEX].reserved = 0;

    colorDetectColorTable[WHITE_COLOR_TABLE_INDEX].blue = 255;
    colorDetectColorTable[WHITE_COLOR_TABLE_INDEX].green = 255;
    colorDetectColorTable[WHITE_COLOR_TABLE_INDEX].red = 255;
    colorDetectColorTable[WHITE_COLOR_TABLE_INDEX].reserved = 0;

    colorDetectColorTable[GREEN_COLOR_TABLE_INDEX].blue = 40;
    colorDetectColorTable[GREEN_COLOR_TABLE_INDEX].green = 255;
    colorDetectColorTable[GREEN_COLOR_TABLE_INDEX].red = 10;
    colorDetectColorTable[GREEN_COLOR_TABLE_INDEX].reserved = 0;

    colorDetectColorTable[PURPLE_COLOR_TABLE_INDEX].blue = 255;
    colorDetectColorTable[PURPLE_COLOR_TABLE_INDEX].green = 50;
    colorDetectColorTable[PURPLE_COLOR_TABLE_INDEX].red = 255;
    colorDetectColorTable[PURPLE_COLOR_TABLE_INDEX].reserved = 0;
}

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

static int find_containing_blob(int w_min, int w_max, int h_last) {
    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];
        if (p_blob->valid) {
            // if this blob is in progress...i.e. the last line added to it
            if (p_blob->bb_y_max + 1 == h_last) {
                //Iterate over segment
                for (size_t j = w_min; j < w_max; ++j) {
                    // If any point is within the x bounding-box of the blob, it belongs to that blob
                    if (j >= p_blob->bb_x_min && j <= p_blob->bb_x_max) {
                        return i;
                    }
                }
            }
        }
    }
    return -1;
}

static void append_blob(int w_min, int w_max, int index) {

    if (w_min < 0 || w_max < 0 || index < 0 || index > COLOR_DETECT_NUM_BLOBS_MAX) {
        perror("Invalid blob or segment");
        return;
    }
    blob_t *p_blob = &detections.blobs[index];

    p_blob->bb_y_max += 1;

    if (w_min < p_blob->bb_x_min) {
        p_blob->bb_x_min = w_min;
    }

    if (w_max > p_blob->bb_x_max) {
        p_blob->bb_x_max = w_max;
    }

    // update pixel count
    p_blob->num_pixels += w_max - w_min;

    // blob has at least two rows, it is complete
    p_blob->complete = true;
}

static int new_blob(int w_min, int w_max, int h) {
    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];
        if (!p_blob->valid) {
            // found empty blob
            p_blob->bb_y_min = h;
            p_blob->bb_y_max = h;
            p_blob->bb_x_min = w_min;
            p_blob->bb_x_max = w_max;
            p_blob->num_pixels = w_max - w_min;
            p_blob->complete = false;
            p_blob->valid = true;
            detections.num_blobs++;
            return i;
        }
    }

    return -1;
}

static int find_largest_blob(void) {
    int largest_pixel_size = 0;
    int largest_blob_index = -1;

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];

        if (p_blob->valid && p_blob->complete) {
            if (p_blob->num_pixels > largest_pixel_size) {
                largest_pixel_size = p_blob->num_pixels;
                largest_blob_index = i;
            }
        }
    }

    return largest_blob_index;
}

static bool bb_overlap(blob_t* p_blob1, blob_t* p_blob2) {
    if ((p_blob1->bb_x_min > p_blob2->bb_x_max) || (p_blob2->bb_x_min > p_blob1->bb_x_max)) {
        return false;
    }
    if ((p_blob1->bb_y_min > p_blob2->bb_y_max) || (p_blob2->bb_y_min > p_blob1->bb_y_max)) {
        return false;
    }
    return true;
}

static void bb_combine(blob_t* p_into, blob_t* p_from) {
    if (p_into->bb_x_min > p_from->bb_x_min) {
        p_into->bb_x_min = p_from->bb_x_min;
    }

    if (p_into->bb_x_max < p_from->bb_x_max) {
        p_into->bb_x_max = p_from->bb_x_max;
    }

    if (p_into->bb_y_min > p_from->bb_y_min) {
        p_into->bb_y_min = p_from->bb_y_min;
    }

    if (p_into->bb_y_max < p_from->bb_y_max) {
        p_into->bb_y_max = p_from->bb_y_max;
    }

    p_into->num_pixels += p_from->num_pixels;

    memset(p_from, 0, sizeof(blob_t));
}

static void combine_blobs_from_largest(void) {
    int largest_blob_index = find_largest_blob();

    if (largest_blob_index >= 0) {
        blob_t* p_largest = &detections.blobs[largest_blob_index];

        for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
            // for every blob but the largest
            if (i != largest_blob_index) {
                blob_t *p_blob = &detections.blobs[i];

                if (p_blob->valid && p_blob->complete) {
                    // check overlap
                    if (bb_overlap(p_largest, p_blob)) {
                        // combine blobs into largest
                        bb_combine(p_largest, p_blob);
                    }
                }
            }
        }
    }
}

static void cull_blobs(int curr_height) {
    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t* p_curr = &detections.blobs[i];

        if (p_curr->bb_y_max < curr_height-1 && (p_curr->num_pixels < MIN_BLOB_PIXELS || !p_curr->complete)) {
            memset((void*)&detections.blobs[i], 0, sizeof(blob_t));
            detections.num_blobs--;
        }
        //printf("%s: blob culled, total blobs %ld\n", __func__, detections.num_blobs);
    }
}

static void new_or_append_blob(int line_min, int line_max, int h) {

    // validate feature horiz size
    if (line_max - line_min < MIN_HORIZ_PIXELS_FOR_FEATURE_LINE) {
        return;
    }

    // determine number of blobs in use
    if (detections.num_blobs == COLOR_DETECT_NUM_BLOBS_MAX) {
        // attempt to cull small blobs
        cull_blobs(h);
    }

    int index = find_containing_blob(line_min, line_max, h);

    if (index < 0) {
        // new blob
        new_blob(line_min, line_max, h);
    } else {
        append_blob(line_min, line_max, index);
    }
}

#if 0
static void debug_blobs(void) {
    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t* p_blob = &detections.blobs[i];
        printf("%s: %lu tp: %u minx: %u maxx: %u miny: %u maxy: %u\n", __func__, i, p_blob->num_pixels, p_blob->bb_x_min, p_blob->bb_x_max, p_blob->bb_y_min, p_blob->bb_y_max);
    }
}
#endif

static int detect_blobs(uint8_t* p_image, int width, int height, uint8_t detect_color) {

    uint8_t *p_curr = p_image;
    int line_min = 0;
    int line_max = 0;
    bool b_in_line = false;

    if (p_curr == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    // clear out blobs
    clear_blobs();

    // Iterate over input image
    for (size_t h=0; h < height; ++h) {
        for (size_t w=0; w < width; ++w) {
            // look for pixel value
            if (*p_curr++ == detect_color) {
                if (!b_in_line) {
                    b_in_line = true;
                    line_min = w;
                }

                if (b_in_line) {
                    line_max = w;
                }
            } else {
                if (b_in_line) {
                    b_in_line = false;
                    new_or_append_blob(line_min, line_max, h);
                }
            }
        }
        // check for in-line at end of row
        if (b_in_line) {
            b_in_line = false;
            new_or_append_blob(line_min, line_max, h);
        }
    }
    // No b_in_line check needed here, since the end of the image is also a row end

    // largest blob accretes all overlapping blobs
    combine_blobs_from_largest();

    return 0;
}

void draw_blob(uint8_t* p_pix, int width, int height, blob_t* p_blob, uint8_t draw_color) {
    int top_line_start = (p_blob->bb_y_min * width) + p_blob->bb_x_min;
    int bottom_line_start = (p_blob->bb_y_max * width) + p_blob->bb_x_min;
    int line_width = p_blob->bb_x_max - p_blob->bb_x_min;
    int line_height = p_blob->bb_y_max - p_blob->bb_y_min;

    // horizontal
    for (size_t i=0; i < line_width; ++i) {
        p_pix[top_line_start+i] = draw_color;
        p_pix[bottom_line_start+i] = draw_color;
    }

    // vertical
    for (size_t i=0; i < line_height; ++i) {
        p_pix[top_line_start+(i*width)] = draw_color;
        p_pix[top_line_start+(i*width)+line_width] = draw_color;
    }
}

void draw_blobs(uint8_t *p_pix, int width, int height, uint8_t draw_color, bool b_largest_only) {
    // Draw bounding box for each blob
    if (b_largest_only) {
        int largest_blob_index = find_largest_blob();

        if (largest_blob_index >= 0) {
            draw_blob(p_pix, width, height, &detections.blobs[largest_blob_index], draw_color);
        }
    } else {
        for (size_t i = 0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
            blob_t *p_blob = &detections.blobs[i];

            if (p_blob->valid && p_blob->complete) {
                draw_blob(p_pix, width, height, p_blob, draw_color);
            }
        }
    }
}

static int num_complete_blobs(void) {
    int num_blobs = 0;

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];
        if (p_blob->valid && p_blob->complete) {
            num_blobs++;
        }
    }
    return num_blobs;
}

bool write_blob_data_to_image(uint8_t* p_image, uint32_t image_size, int stride, int height) {
    bool retVal = false;

    if (!p_image || image_size != stride * height || stride <= 0 || height <= 0) {
        return false;
    }

    // only write complete blobs, check if there are any to write
    int num_blobs = num_complete_blobs();

    if (num_blobs == 0) {
        return true;
    }

    // Allocate a buffer equal to the size of the first row
    char *p_line_buf = (char*)calloc(stride , 1);

    if (!p_line_buf) {
        return false;
    }

    // keep track of remaining space in first image line
    int blob_string_remaining = stride;

    // write num blobs first
    int num_blob_string_len = snprintf(p_line_buf, blob_string_remaining, "%d", num_blobs);

    blob_string_remaining -= num_blob_string_len;

    int blob_index = 0;

    // add each complete and valid blob to string
    for (size_t i = 0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];

        // validate blob
        if (p_blob->valid && p_blob->complete && p_blob->num_pixels != 0) {
            char blob_string[128] = { 0 }; // 10 characters per integer plus 6 commas plus null terminator

            int blob_string_len = snprintf(blob_string, 128, ",%d,%d,%d,%d,%d,%d", blob_index, p_blob->bb_x_min, p_blob->bb_x_max,
                                           p_blob->bb_y_min, p_blob->bb_y_max, p_blob->num_pixels);

            // ensure room to append in p_line_buf
            if (blob_string_remaining < blob_string_len) {
                break;
            }

            // write blob string
            strncat(p_line_buf, blob_string, blob_string_len);

            blob_string_remaining -= blob_string_len;
            blob_index++;
        }
    }

    // append a line-feed
    strcat(p_line_buf, "\n");

    // replace the first line in the image
    memcpy(p_image, p_line_buf, stride);

    free(p_line_buf);
    return retVal;
}

bool write_single_blob_data_to_image(blob_t* p_blob, uint8_t* p_image, uint32_t image_size, int stride, int height) {
    bool retVal = false;

    if (!p_blob || !p_image || image_size != stride * height || stride <= 0 || height <= 0) {
        return false;
    }

    // Allocate a buffer equal to the size of the first row
    char *p_line_buf = (char*)calloc(stride , 1);

    if (!p_line_buf) {
        return false;
    }

    // validate blob
    if (!p_blob->valid || !p_blob->complete || p_blob->num_pixels == 0) {
        goto _cleanUp;
    }

    // serialize blob to line_buf
    sprintf(p_line_buf, "1,%u,%u,%u,%u,%u,%u\n", 0, p_blob->bb_x_min, p_blob->bb_x_max, p_blob->bb_y_min, p_blob->bb_y_max, p_blob->num_pixels);

    // replace the first line in the image
    memcpy(p_image, p_line_buf, stride);

    _cleanUp:
    free(p_line_buf);
    return retVal;
}

// assumes pixels packed RGBRGBRGB...3 bytes per pixel
void rgb_color_detection(uint8_t *p_pix,
                        uint32_t pixSize,
                        int width,
                        int height,
                        detect_color_t *p_detect_color,
                        float detect_tolerance,
                        bool b_write_image,
                        bool b_write_detection,
                        bool b_write_all_detections) {

    size_t detect_image_size = width * height;

    if (pixSize != width * height * 3) {
        printf("%s: invalid pixel buffer size (%u != %d * %d)\n", __func__, pixSize, width, height);
        return;
    }

    uint8_t *p_detect_image_start = (uint8_t *) malloc(detect_image_size);
    uint8_t *p_detect_image = p_detect_image_start;

    // iterate over input image buffer and write 0 if specified color not detected, 1 if detected
    uint8_t *p_input = p_pix;

    for (uint32_t h = 0; h < height; ++h) {
        for (uint32_t w = 0; w < width; ++w) {
            // decode one pixel
            uint8_t blue = *p_input++;
            uint8_t green = *p_input++;
            uint8_t red = *p_input++;

            if (rgb_match(p_detect_color, red, green, blue, detect_tolerance)) {
                *p_detect_image++ = DETECT_COLOR_TABLE_INDEX;
            } else {
                *p_detect_image++ = NO_DETECT_COLOR_TABLE_INDEX;
            }
        }
    }

    detect_blobs(p_detect_image_start, width, height, DETECT_COLOR_TABLE_INDEX);

    if (b_write_image) {
        draw_blobs(p_detect_image_start, width, height, DRAW_BLOB_COLOR_TABLE_INDEX, false);
        //debug_blobs();
    }

    // blob detection image write
    if (b_write_detection) {
        if (b_write_all_detections) {
            write_blob_data_to_image(p_pix, pixSize, width * 3, height);
        } else {
            // write the largest blob
            int index = find_largest_blob();
            if (index >= 0) {
                blob_t *p_largest = &detections.blobs[index];
                if (p_largest) {
                    write_single_blob_data_to_image(p_largest, p_pix, pixSize, width * 3, height);
                }
            }
        }
    }

    if (b_write_image) {
        FILE *p_file = fopen(color_detect_file_name, "w+");

        if (p_file == NULL) {
            perror("Can't write output image file.");
            free(p_detect_image_start);
            return;
        }

        // write image
        bmWriteBitmapWithColorTable(p_file, width, height, colorDetectColorTable, sizeof(colorDetectColorTable), p_detect_image_start, detect_image_size);

        fflush(p_file);
        fclose(p_file);

        /* Now that write is complete, rename the file */
        rename(color_detect_file_name, color_detect_file_rename);
    }

    free(p_detect_image_start);
    p_detect_image_start = NULL;
}
