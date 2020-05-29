//
// Created by rsnook on 5/28/20.
//
#ifndef _COLOR_DETECT_H
#define _COLOR_DETECT_H

#include <stdint.h>
#include <stdbool.h>

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

// Takes a detect_color_t and calculates normalized and filter normalized values
bool calcNorms(detect_color_t* p_detect_color);

// Checks if a pixel red-green-blue value matches the specified detect_color within tolerance (percent)
bool rgb_match(detect_color_t *p_detect_color, uint8_t red, uint8_t green, uint8_t blue, float tolerance);

// assumes pixels packed RGBRGBRGB...3 bytes per pixel
void rgb_color_detection(uint8_t *p_pix, uint32_t pixSize, int width, int height, detect_color_t *p_detect_color, float detect_tolerance);

#endif //_COLOR_DETECT_H
