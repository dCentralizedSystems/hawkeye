//
// Created by rsnook on 5/28/20.
//
#ifndef _COLOR_DETECT_H
#define _COLOR_DETECT_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of blobs detected
#define COLOR_DETECT_NUM_BLOBS_MAX      (10)

// Color detection structure
typedef struct {
    float red;
    float green;
    float blue;
    float norm;
    float red_norm;
    float green_norm;
    float blue_norm;
    float gr_norm;
    float gb_norm;
    float rb_norm;
} detect_color_t;

// blob data structure
typedef struct {
    // validity flag
    bool valid;

    // bounding-box
    uint32_t bb_x_min;
    uint32_t bb_x_max;
    uint32_t bb_y_min;
    uint32_t bb_y_max;

    // centroid
    uint32_t cent_x;
    uint32_t cent_y;

    // color index
    uint32_t color_index;

    // number of pixels
    uint32_t num_pixels;

    // completion flag
    bool complete;
} blob_t;


typedef struct {
    int color_count;
    detect_color_t* p_detect_colors;
    float tolerance;
    int min_detect_conf;
    bool b_write_image;
    char detection_image_file_name[256];
    bool b_write_detection;
} detect_params_t;

void colorDetectInit(void);

// Takes a detect_color_t and calculates normalized and filter normalized values
bool calcNorms(detect_color_t* p_detect_color);

// Set detect color
void setDetectColor(detect_color_t* p_detect_color, uint32_t index);

// Checks if a pixel red-green-blue value matches the specified detect_color within tolerance (percent)
bool rgb_match(detect_color_t *p_detect_color, uint8_t red, uint8_t green, uint8_t blue, float tolerance);

void build_detect_params(detect_params_t *p_params,
                         int color_count,
                         float detect_tolerance,
                         int min_detect_conf,
                         bool b_write_image,
                         bool b_write_detection,
                         const char* color_detect_image_path,
                         const char* color_detect_image_name);

// assumes pixels packed RGBRGBRGB...3 bytes per pixel
const char * rgb_color_detection(uint8_t *p_pix, int width, int height, detect_params_t *p_detect_params);

// Retrieve blob by index from detection results
blob_t* get_blob(size_t index);

// Get number of blobs from detection results
size_t get_num_blobs(void);

#endif //_COLOR_DETECT_H
