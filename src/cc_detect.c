//
// Created by rsnook on 11/5/20.
//
#include <string.h>

#include "cc_detect.h"

// detector configuration default values
#define CC_DETECT_MAX_NUM_STRIPES           (21)
#define CC_DETECT_MAX_STRIPE_WIDTH_PERCENT  (10)
#define CC_DETECT_MIN_STRIPE_WIDTH_PERCENT  (1)

#if 0
// Detector configuration
typedef struct {
    // threshold calculation
    uint32_t threshold_accumulator;
    uint32_t num_pixels_processed;

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
} cc_detect_params_t;
#endif /* #if 0 */

void cc_detect_init_image(cc_detect_params_t* p_params) {
    if (!p_params) {
        return;
    }

    // clear detect structure
    memset(p_params, 0, sizeof(cc_detect_params_t));
}

static void new_stripe(cc_detect_params_t* p_params, uint32_t start, cc_detect_stripe_type_t type) {
    if (p_params->num_stripes < CC_DETECT_MAX_STRIPES_PER_LINE) {
        p_params->stripe_info[p_params->num_stripes].start = start;
        p_params->stripe_info[p_params->num_stripes].end = start;
        p_params->stripe_info[p_params->num_stripes].type = type;
        p_params->stripe_info[p_params->num_stripes].length = 1;

        ++p_params->num_stripes;
    }
}

static void process_pixel(cc_detect_params_t* p_params, uint32_t index, cc_detect_stripe_type_t type) {
    // increment current stripe if pixel types matches the stripe in progress type
    if (type == p_params->stripe_info[p_params->num_stripes].type) {
        p_params->stripe_info[p_params->num_stripes].end = index;
        p_params->stripe_info[p_params->num_stripes].length += 1;
    } else { // different type of pixel, could be none or zero
        new_stripe(p_params, index, type);
    }
}

void cc_detect_process_line(cc_detect_params_t* p_params, uint8_t* p_line, unsigned int width) {
    if (!p_line || !p_params) {
        return;
    }

    // copy current stripes to previous stripes since we're about to start a new detection
    memcpy(&p_params->prev_stripe_info[0], &p_params->stripe_info[0], sizeof(cc_detect_stripe_info_t) * CC_DETECT_MAX_STRIPES_PER_LINE);
    p_params->prev_num_stripes = p_params->num_stripes;

    // stripe index re-starts at zero for each new line.
    memset(&p_params->stripe_info[0], 0, sizeof(cc_detect_stripe_info_t) * CC_DETECT_MAX_STRIPES_PER_LINE);
    p_params->num_stripes = 0;

    uint8_t threshold = p_params->threshold;

    // iterate over line
    for (int i=0; i < width; ++i) {
        cc_detect_stripe_type_t pixel_type = (p_line[i] < threshold) ? STRIPE_TYPE_ZERO : STRIPE_TYPE_ONE;
        process_pixel(p_params, i, pixel_type);

        // Update average accumulator for threshold calculation
        p_params->threshold_accumulator += p_line[i];
    }
    p_params->num_pixels_processed += width;
}

void cc_detect_end_frame(cc_detect_params_t* p_params) {
    if (!p_params) {
        return;
    }

    if (p_params->num_pixels_processed) {
        p_params->threshold = p_params->threshold_accumulator / p_params->num_pixels_processed;
    } else {
        p_params->threshold = UINT8_MAX / 2;
    }

    p_params->num_pixels_processed = 0;
    p_params->threshold_accumulator = 0;
}

const char* cc_detect_get_result_string(cc_detect_params_t* p_params) {
    static char default_msg[] = "Default, replace me";
    return default_msg;
}


