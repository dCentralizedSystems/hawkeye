//
// Created by rsnook on 11/5/20.
//

#ifndef _CC_DETECT_H
#define _CC_DETECT_H

#include <stdint.h>
#include <stdbool.h>

// Fiducial limits
#define CC_DETECT_MAX_FIDUCIALS         (20)

// Stripe limits per line
#define CC_DETECT_MAX_STRIPES_PER_LINE  (50)

// Stripe type
typedef enum {
    STRIPE_TYPE_NONE = 0,
    STRIPE_TYPE_ZERO,
    STRIPE_TYPE_ONE
} cc_detect_stripe_type_t;

// Stripe information
typedef struct {
    cc_detect_stripe_type_t type;
    uint32_t start;
    uint32_t end;
    uint32_t length;
} cc_detect_stripe_info_t;

// Fiducial information
typedef struct {
    uint32_t center_x;
    uint32_t center_y;
    uint32_t bb_x_min;
    uint32_t bb_x_max;
    uint32_t bb_y_min;
    uint32_t bb_y_max;
} cc_fiducial_info_t;

// Detector configuration
typedef struct {
    // threshold calculation
    uint32_t threshold_accumulator;
    uint32_t num_pixels_processed;
    uint8_t threshold;

    // stripe detector limits
    uint32_t max_num_stripes;
    uint32_t max_stripe_width_percent;
    uint32_t min_stripe_width_percent;

    // image configuration
    uint32_t image_width;
    uint32_t image_height;

    // detector configuration
    uint32_t num_fiducial_stripes;

    // vertical center configuration
    uint32_t stripe_width_allowed_variance;

    // stripe detections
    cc_detect_stripe_info_t stripe_info[CC_DETECT_MAX_STRIPES_PER_LINE];
    uint32_t num_stripes;
    cc_detect_stripe_info_t prev_stripe_info[CC_DETECT_MAX_STRIPES_PER_LINE];
    uint32_t prev_num_stripes;

    // fiducial detections
    cc_fiducial_info_t fiducial_info[CC_DETECT_MAX_FIDUCIALS];
} cc_detect_params_t;

void cc_detect_init_image(cc_detect_params_t* p_params);
void cc_detect_process_line(cc_detect_params_t* p_params, uint8_t* p_line, unsigned int width);
void cc_detect_end_frame(cc_detect_params_t* p_params);
const char* cc_detect_get_result_string(cc_detect_params_t* p_params);

#endif //_CC_DETECT_H
