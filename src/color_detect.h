//
// Created by rsnook on 5/28/20.
//
#ifndef _COLOR_DETECT_H
#define _COLOR_DETECT_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of blobs detected
#define COLOR_DETECT_NUM_BLOBS_MAX      (5)

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

    // number of pixels
    uint32_t num_pixels;

    // completion flag
    bool complete;
} blob_t;

void colorDetectInit(void);

// Takes a detect_color_t and calculates normalized and filter normalized values
bool calcNorms(detect_color_t* p_detect_color);

// Checks if a pixel red-green-blue value matches the specified detect_color within tolerance (percent)
bool rgb_match(detect_color_t *p_detect_color, uint8_t red, uint8_t green, uint8_t blue, float tolerance);

// assumes pixels packed RGBRGBRGB...3 bytes per pixel
void rgb_color_detection(uint8_t *p_pix, uint32_t pixSize, int width, int height, detect_color_t *p_detect_color, float detect_tolerance, bool b_write_image, bool b_write_detection);

// Retrieve blob by index from detection results
blob_t* get_blob(size_t index);

// Get number of blobs from detection results
size_t get_num_blobs(void);

#endif //_COLOR_DETECT_H
