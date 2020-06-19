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

#define BLOB_ALLOWED_ROW_MISS_PERCENT       (0.05f)
#define BLOB_ALLOWED_COLUMN_MISS_PERCENT    (0.05f)

#define WHITE_COLOR_TABLE_INDEX             (3)
#define BLACK_COLOR_TABLE_INDEX             (2)
#define GREEN_COLOR_TABLE_INDEX             (4)
#define PURPLE_COLOR_TABLE_INDEX            (0)
#define YELLOW_COLOR_TABLE_INDEX            (1)
#define ORANGE_COLOR_TABLE_INDEX            (5)

#define NO_DETECT_COLOR_TABLE_INDEX         (BLACK_COLOR_TABLE_INDEX)

#define MAX_DETECT_COLORS                   (2)

#define BLOB_STRING_MAX_LENGTH              (1024)

static uint32_t detect_color_table[MAX_DETECT_COLORS] = { PURPLE_COLOR_TABLE_INDEX, YELLOW_COLOR_TABLE_INDEX };
static uint32_t draw_color_table[MAX_DETECT_COLORS] = { ORANGE_COLOR_TABLE_INDEX, GREEN_COLOR_TABLE_INDEX };
static detect_color_t detect_colors[MAX_DETECT_COLORS] = { 0 };

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

    colorDetectColorTable[YELLOW_COLOR_TABLE_INDEX].blue = 0;
    colorDetectColorTable[YELLOW_COLOR_TABLE_INDEX].green = 255;
    colorDetectColorTable[YELLOW_COLOR_TABLE_INDEX].red = 255;
    colorDetectColorTable[YELLOW_COLOR_TABLE_INDEX].reserved = 0;

    colorDetectColorTable[ORANGE_COLOR_TABLE_INDEX].blue = 0;
    colorDetectColorTable[ORANGE_COLOR_TABLE_INDEX].green = 165;
    colorDetectColorTable[ORANGE_COLOR_TABLE_INDEX].red = 255;
    colorDetectColorTable[ORANGE_COLOR_TABLE_INDEX].reserved = 0;
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

void setDetectColor(detect_color_t* p_detect_color, uint32_t index) {
    if (index < MAX_DETECT_COLORS) {
        memcpy(&detect_colors[index], p_detect_color, sizeof(detect_color_t));
    }
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

    // color ratio-based match
    if (in_pix_color.gr_norm > p_detect_color->gr_norm * norm_min && in_pix_color.gr_norm < p_detect_color->gr_norm * norm_max) {
        if (in_pix_color.gb_norm > p_detect_color->gb_norm * norm_min && in_pix_color.gb_norm < p_detect_color->gb_norm * norm_max) {
            if (in_pix_color.rb_norm > p_detect_color->rb_norm * norm_min && in_pix_color.rb_norm < p_detect_color->rb_norm * norm_max) {
                return true;
            }
        }
    }

//#define ENABLE_VALUE_BASED_MATCH
#ifdef ENABLE_VALUE_BASED_MATCH
    float value_match_tolerance = 0.1f;
    float value_match_tolerance_min = 1.0f - value_match_tolerance;
    float value_match_tolerance_max = 1.0f + value_match_tolerance;

    if (in_pix_color.red > p_detect_color->red * value_match_tolerance_min && in_pix_color.red < p_detect_color->red * value_match_tolerance_max) {
        if (in_pix_color.green > p_detect_color->green * value_match_tolerance_min && in_pix_color.green < p_detect_color->green * value_match_tolerance_max) {
            if (in_pix_color.blue > p_detect_color->blue * value_match_tolerance_min && in_pix_color.blue < p_detect_color->blue * value_match_tolerance_max) {
                return true;
            }
        }
    }
#endif
    return false;
}

static void clear_blobs(void) {
    detections.num_blobs = 0;

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        memset(&detections.blobs[i], 0, sizeof(blob_t));
    }
}

static int find_containing_blob(int w_min, int w_max, int h_last, int image_width, int image_height, int detect_color_index) {
    int allowed_row_miss = (int)(BLOB_ALLOWED_ROW_MISS_PERCENT * ((float)image_height));
    int allowed_column_miss = (int)(BLOB_ALLOWED_COLUMN_MISS_PERCENT * ((float)image_width));

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];

        if (p_blob->valid) {
            // check that the detect color matches
            if (detect_color_index == p_blob->color_index) {
                int x_min = (p_blob->bb_x_min - allowed_column_miss < 0) ? 0 : p_blob->bb_x_min - allowed_column_miss;
                int x_max = (p_blob->bb_x_max + allowed_column_miss > image_width) ? image_width : p_blob->bb_x_max +
                                                                                                   allowed_column_miss;
                int y_max = (p_blob->bb_y_max + allowed_row_miss > image_height) ? image_height : p_blob->bb_y_max +
                                                                                                  allowed_row_miss;

                // if this blob is in progress...i.e. the last line added to it
                if (y_max >= h_last) {
                    //Iterate over segment
                    for (size_t j = w_min; j < w_max; ++j) {
                        // If any point is within the x bounding-box of the blob, it belongs to that blob
                        if (j >= x_min && j <= x_max) {
                            return i;
                        }
                    }
                }
            }
        }
    }
    return -1;
}

static void append_blob(int w_min, int w_max, int h_last, int index, uint32_t detect_color_index) {

    if (w_min < 0 || w_max < 0 || index < 0 || index > COLOR_DETECT_NUM_BLOBS_MAX) {
        perror("Invalid blob or segment");
        return;
    }
    blob_t *p_blob = &detections.blobs[index];

    if (detect_color_index != p_blob->color_index) {
        return;
    }

    p_blob->bb_y_max = h_last;

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

static int new_blob(int w_min, int w_max, int h, uint32_t detect_color_index) {
    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];
        if (!p_blob->valid) {
            // found empty blob
            p_blob->bb_y_min = h;
            p_blob->bb_y_max = h;
            p_blob->bb_x_min = w_min;
            p_blob->bb_x_max = w_max;
            p_blob->num_pixels = w_max - w_min;
            p_blob->color_index = detect_color_index;
            p_blob->complete = false;
            p_blob->valid = true;
            detections.num_blobs++;
            return i;
        }
    }

    return -1;
}

static int find_largest_blob(uint32_t detect_color_index) {
    int largest_pixel_size = 0;
    int largest_blob_index = -1;

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];

        if (detect_color_index != p_blob->color_index) {
            continue;
        }

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
    if (p_blob1->color_index != p_blob2->color_index) {
        return false;
    }
    
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

static void combine_blobs_from_largest(uint32_t detect_color_count) {
    for (uint32_t color_index=0; color_index < detect_color_count; ++color_index) {
        int largest_blob_index = find_largest_blob(color_index);

        if (largest_blob_index >= 0) {
            blob_t *p_largest = &detections.blobs[largest_blob_index];

            for (size_t i = 0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
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
}

static bool blob_area_over_threshold(blob_t* p_blob, int image_width, int image_height, int min_detect_conf) {

    // calculate blob bounding-box num pixels
    if (p_blob == NULL) {
        return false;
    }

    // minimum allowed number of pixels for a blob is min_detect_conf 10,000ths of the total number of image pixels
    int min_blob_pixels = ((float)min_detect_conf / 10000.0f) * image_width * image_height;

    int blob_bb_area = (p_blob->bb_x_max - p_blob->bb_x_min) * (p_blob->bb_y_max - p_blob->bb_y_min);

    return (blob_bb_area > min_blob_pixels);
}

static void cull_blobs(int curr_height, int image_width, int image_height, int min_detect_conf) {

    for (size_t i=0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t* p_curr = &detections.blobs[i];

        if (!p_curr->valid) {
            continue;
        }

        float allowed_row_miss = BLOB_ALLOWED_ROW_MISS_PERCENT * ((float)image_height);

        // last allowed row able to be added to the current blob
        int last_allowed_row = p_curr->bb_y_max  + (int)allowed_row_miss > image_height ? image_height :  (int)(p_curr->bb_y_max  + (int)allowed_row_miss);

        if (last_allowed_row < curr_height) {
            if (!blob_area_over_threshold(p_curr, image_width, image_height, min_detect_conf) || p_curr->num_pixels < MIN_BLOB_PIXELS || !p_curr->complete) {
                memset((void*)&detections.blobs[i], 0, sizeof(blob_t));
                detections.num_blobs--;
            }
        }
    }
}

static void new_or_append_blob(int line_min, int line_max, int h, int image_width, int image_height, uint32_t detect_color_index) {

    // validate feature horiz size
    if (line_max - line_min < MIN_HORIZ_PIXELS_FOR_FEATURE_LINE) {
        return;
    }

    int index = find_containing_blob(line_min, line_max, h, image_width, image_height, detect_color_index);

    if (index < 0) {
        if (detections.num_blobs < COLOR_DETECT_NUM_BLOBS_MAX) {
            // new blob
            new_blob(line_min, line_max, h, detect_color_index);
        }
    } else {
        append_blob(line_min, line_max, h, index, detect_color_index);
    }
}

static int detect_blobs(uint8_t* p_image, int width, int height, int detect_color_count, int min_detect_conf) {

    uint8_t *p_curr = p_image;
    int line_min = 0;
    int line_max = 0;
    bool b_in_line = false;
    uint32_t line_color_index = 0;

    if (p_curr == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    // clear out blobs
    clear_blobs();

    // Iterate over input image
    for (size_t h=0; h < height; ++h) {
        for (size_t w=0; w < width; ++w) {
            uint8_t curr_color_index = *p_curr++;

            if (curr_color_index == NO_DETECT_COLOR_TABLE_INDEX) {
                if (b_in_line) {
                    b_in_line = false;
                    new_or_append_blob(line_min, line_max, h, width, height, line_color_index);
                }
            } else {
                if (b_in_line) {
                    if (curr_color_index == line_color_index) {
                        line_max = w;
                    } else {
                        // not background, but not current line color, end current line and start new line
                        new_or_append_blob(line_min, line_max, h, width, height, line_color_index);

                        // start new line
                        b_in_line = true;
                        line_min = w;
                        line_max = w;
                        line_color_index = curr_color_index;
                    }
                } else {
                    // start new line
                    b_in_line = true;
                    line_min = w;
                    line_max = w;
                    line_color_index = curr_color_index;
                }
            }
        }
        if (b_in_line) {
            b_in_line = false;
            new_or_append_blob(line_min, line_max, h, width, height, line_color_index);
        }
        cull_blobs(h, width, height, min_detect_conf);
    }
    // No b_in_line check needed here, since the end of the image is also a row end

    // largest blob subsumes all overlapping blobs
    combine_blobs_from_largest(detect_color_count);

    return 0;
}

void draw_blob(uint8_t* p_pix, int width, int height, blob_t* p_blob) {
    int top_line_start = (p_blob->bb_y_min * width) + p_blob->bb_x_min;
    int bottom_line_start = (p_blob->bb_y_max * width) + p_blob->bb_x_min;
    int line_width = p_blob->bb_x_max - p_blob->bb_x_min;
    int line_height = p_blob->bb_y_max - p_blob->bb_y_min;

    uint8_t draw_color = draw_color_table[p_blob->color_index % 2];

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

void draw_blobs(uint8_t *p_pix, int width, int height, bool b_largest_only, uint32_t detect_color_count) {
    // Draw bounding box for each blob
    if (b_largest_only) {
        for (uint32_t i=0; i < detect_color_count; ++i) {
            int largest_blob_index = find_largest_blob(i);

            if (largest_blob_index >= 0) {
                draw_blob(p_pix, width, height, &detections.blobs[largest_blob_index]);
            }
        }
    } else {
        for (size_t i = 0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
            blob_t *p_blob = &detections.blobs[i];

            if (p_blob->valid && p_blob->complete) {
                draw_blob(p_pix, width, height, p_blob);
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

const char* get_blob_data_string() {
    static char blobs_string[BLOB_STRING_MAX_LENGTH];

    memset(blobs_string, 0, BLOB_STRING_MAX_LENGTH);

    // only write complete blobs, check if there are any to write
    int num_blobs = num_complete_blobs();

    if (num_blobs == 0) {
        return NULL;
    }

    // keep track of remaining space in first image line
    int blob_string_remaining = BLOB_STRING_MAX_LENGTH;

    // write num blobs first
    int num_blob_string_len = snprintf(blobs_string, blob_string_remaining, "%d", num_blobs);

    blob_string_remaining -= num_blob_string_len;

    int blob_index = 0;

    // add each complete and valid blob to string
    for (size_t i = 0; i < COLOR_DETECT_NUM_BLOBS_MAX; ++i) {
        blob_t *p_blob = &detections.blobs[i];

        // validate blob
        if (p_blob->valid && p_blob->complete && p_blob->num_pixels != 0) {
            char blob_string[128] = { 0 }; // 10 characters per integer plus 6 commas plus null terminator

            int blob_string_len = snprintf(blob_string, 128, ",%d,%d,%d,%d,%d,%d,%d", blob_index, p_blob->bb_x_min, p_blob->bb_x_max,
                                           p_blob->bb_y_min, p_blob->bb_y_max, p_blob->color_index, p_blob->num_pixels);

            // ensure room to append in p_line_buf
            if (blob_string_remaining < blob_string_len) {
                break;
            }

            // write blob string
            strncat(blobs_string, blob_string, blob_string_len);

            blob_string_remaining -= blob_string_len;
            blob_index++;
        }
    }

    if (blob_string_remaining > 0) {
        // append a line-feed
        strcat(blobs_string, "\n");
    }

    return blobs_string;
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

void build_detect_params(detect_params_t *p_params,
                        int color_count,
                        float detect_tolerance,
                        int min_detect_conf,
                        bool b_write_image,
                        bool b_write_detection,
                        const char* color_detect_image_path,
                        const char* color_detect_image_name) {
    if (!p_params) {
        return;
    }

    p_params->color_count = color_count;
    p_params->p_detect_colors = &detect_colors[0];
    p_params->tolerance = detect_tolerance;
    p_params->min_detect_conf = min_detect_conf;
    p_params->b_write_image = b_write_image;
    p_params->b_write_detection = b_write_detection;
    memset(p_params->detection_image_file_name, 0, 256);
    snprintf(p_params->detection_image_file_name, 256, "%s/%s", color_detect_image_path, color_detect_image_name);
}


// assumes pixels packed RGBRGBRGB...3 bytes per pixel
const char * rgb_color_detection(uint8_t *p_pix, int width, int height, detect_params_t *p_detect_params) {

    if (!p_detect_params) {
        return NULL;
    }

    size_t detect_image_size = width * height;

    static uint8_t *p_detect_image_start = NULL;

    if (p_detect_image_start == NULL) {
        p_detect_image_start = (uint8_t *) malloc(detect_image_size);
    }
    
    uint8_t *p_detect_image = p_detect_image_start;

    // iterate over input image buffer and write 0 if specified color not detected, 1 if detected
    uint8_t *p_input = p_pix;

    for (uint32_t h = 0; h < height; ++h) {
        for (uint32_t w = 0; w < width; ++w) {
            // decode one pixel
            uint8_t red = *p_input++;
            uint8_t green = *p_input++;
            uint8_t blue = *p_input++;

            bool b_set = false;

            for (uint32_t dci=0; dci < p_detect_params->color_count; ++dci) {
                detect_color_t* p_detect_color = &p_detect_params->p_detect_colors[dci];
                if (rgb_match(p_detect_color, red, green, blue, p_detect_params->tolerance)) {
                    *p_detect_image++ = detect_color_table[dci];
                    b_set = true;
                    break;
                }
            }

            if (!b_set) {
                *p_detect_image++ = NO_DETECT_COLOR_TABLE_INDEX;
            }
        }
    }

    detect_blobs(p_detect_image_start, width, height, p_detect_params->color_count, p_detect_params->min_detect_conf);

    if (p_detect_params->b_write_image) {
        draw_blobs(p_detect_image_start, width, height, false, p_detect_params->color_count);
    }

    if (p_detect_params->b_write_image) {
        static char color_detect_file_temp_name[512] = { 0 };
        static char color_detect_file_name[512] = { 0 };

        if (color_detect_file_temp_name[0] == 0 || color_detect_file_name[0] == 0) {
            snprintf(color_detect_file_temp_name, 257, "%s~", p_detect_params->detection_image_file_name);
            snprintf(color_detect_file_name, 257,"%s", p_detect_params->detection_image_file_name);
        }

        FILE *p_file = fopen(color_detect_file_temp_name, "w+");

        if (p_file != NULL) {

            // write image
            bmWriteBitmapWithColorTable(p_file, width, height, colorDetectColorTable, sizeof(colorDetectColorTable),
                                        p_detect_image_start, detect_image_size);

            fflush(p_file);
            fclose(p_file);

            /* Now that write is complete, rename the file */
            rename(color_detect_file_temp_name, color_detect_file_name);
        }
    }

    //free(p_detect_image_start);
    //p_detect_image_start = NULL;

    return get_blob_data_string();
}
