//
// Created by rsnook on 11/10/20.
//
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "bitmap.h"
#include "stripe_filter.h"

typedef enum {
    SF_THRESHOLD_STATE_ABOVE,
    SF_THRESHOLD_STATE_BELOW
} sf_threshold_state_t;

typedef struct {
    sf_threshold_state_t state;
    uint8_t value;
} sf_threshold_t;

struct sf_filter;

/* Define a filter-function type */
typedef uint8_t (*sf_filter_fn_t)(struct sf_filter*, uint8_t);

struct sf_filter {
    uint8_t filter_elem[SF_FILTER_NUM_ELEM];
    sf_filter_fn_t filter_fn;
};

/* Grayscale image color table */
static rgbColorTableEntry stripeFilterColorTable[256] = { 0 };

/* Grayscale temp file name */
static const char sf_gray_temp_file_name[] = "./sf_temp_file.bmp";

/* Set filter calculation function */
void sf_set_filter_function(struct sf_filter* p_filter, sf_filter_fn_t fn) {
    if (!p_filter) {
        return;
    }

    p_filter->filter_fn = fn;
}

/* Calculate filter output */
uint8_t sf_boxcar_filter(struct sf_filter* p_filter, uint8_t input) {
    if (!p_filter) {
        return 0;
    }

    uint32_t accum = 0;

    for (size_t i=0; i < SF_FILTER_NUM_ELEM; ++i) {
        accum += p_filter->filter_elem[i];

        /* Copy-down filter values */
        if (i != 0) {
            p_filter->filter_elem[i-1] = p_filter->filter_elem[i];
        }
    }

    /* Insert input into filter */
    p_filter->filter_elem[SF_FILTER_NUM_ELEM-1] = input;

    accum /= SF_FILTER_NUM_ELEM;

    return (uint8_t) accum;
}

static bool sf_add_gradient(sf_gradient_list_t* p_grad_list, sf_gradient_info_t* p_grad_info) {
    if (!p_grad_list || !p_grad_info) {
        return false;
    }

    if (p_grad_list->num_elem >= SF_MIN_STRIPE_WIDTH-1) {
        return false;
    }

    /* Copy to the next element */
    p_grad_list->gradient_list[p_grad_list->num_elem].x_coord = p_grad_info->x_coord;
    p_grad_list->gradient_list[p_grad_list->num_elem].y_coord = p_grad_info->y_coord;
    p_grad_list->gradient_list[p_grad_list->num_elem].value = p_grad_info->value;
    p_grad_list->gradient_list[p_grad_list->num_elem].type = p_grad_info->type;
    ++p_grad_list->num_elem;

    return true;
}

/* Process a single line of length, len.  Perform a locally-adaptive threshold
 * using a Nx1 horizontal kernel to threshold each pixel.  Look for
 * strong gradients using the adaptive threshold.  Return a list of
 * candidate gradient locations.
 */
bool sf_find_gradients(sf_gradient_list_t* p_grad_list, uint8_t* p_gray, uint32_t len, uint32_t y_coord) {
    if (!p_grad_list || !p_gray || !len || len <= SF_FILTER_NUM_ELEM) {
        return false;
    }

    static struct sf_filter filter;
    memset(&filter, 0, sizeof(struct sf_filter));

    /* No initial gradient detections */
    p_grad_list->num_elem = 0;

    /* Start below threshold */
    sf_threshold_t curr_threshold;
    curr_threshold.state = SF_THRESHOLD_STATE_BELOW;
    curr_threshold.value = p_gray[0];

    /* Storage for gradients to be added */
    sf_gradient_info_t grad_info;

    /* Set the function used to evaluate the filter */
    if (filter.filter_fn == NULL) {
        sf_set_filter_function(&filter, &sf_boxcar_filter);
    }

    /* Start at filter-width from the left image edge, populate the filter */
    size_t i;
    for (i=0; i < SF_FILTER_NUM_ELEM; ++i) {
        filter.filter_fn(&filter, UINT8_MAX >> 1);
    }

    /* Iterate over line (staring from filter width) */
    for (;i < len; ++i) {
        /* Calculate the threshold value at the current pixel and add the current pixel value to the filter */
        uint16_t filter_threshold_value = filter.filter_fn(&filter, p_gray[i]);
        uint8_t filter_min = (filter_threshold_value > SF_GRADIENT_MIN_CHANGE) ? filter_threshold_value - SF_GRADIENT_MIN_CHANGE : SF_GRADIENT_MIN_CHANGE;
        uint8_t filter_max = (filter_threshold_value + SF_GRADIENT_MIN_CHANGE > UINT8_MAX) ? UINT8_MAX - SF_GRADIENT_MIN_CHANGE: filter_threshold_value + SF_GRADIENT_MIN_CHANGE;

        /* Check for gradients */
        if (p_gray[i] > filter_max) {
            /* Above threshold */
            if (curr_threshold.state != SF_THRESHOLD_STATE_ABOVE) {
                /* Positive gradient */
                curr_threshold.state = SF_THRESHOLD_STATE_ABOVE;

                /* Build information about gradient */
                grad_info.value = p_gray[i];
                grad_info.type = SF_GRADIENT_POSITIVE;
                grad_info.x_coord = i;
                grad_info.y_coord = y_coord;

                /* Add to gradient list */
                sf_add_gradient(p_grad_list, &grad_info);
            }
        } else if (p_gray[i] < filter_min) {
            /* Below threshold */
            if (curr_threshold.state != SF_THRESHOLD_STATE_BELOW) {
                /* Negative gradient */
                curr_threshold.state = SF_THRESHOLD_STATE_ABOVE;

                /* Build information about gradient */
                grad_info.value = p_gray[i];
                grad_info.type = SG_GRADIENT_NEGATIVE;
                grad_info.x_coord = i;
                grad_info.y_coord = y_coord;

                /* Add to gradient list */
                sf_add_gradient(p_grad_list, &grad_info);
            }
        }
        curr_threshold.value = p_gray[i];
    }

    return true;
}

static bool sf_add_stripe(sf_stripe_list_t* p_stripe_list, sf_stripe_info_t* p_stripe_info) {
    if (!p_stripe_list || !p_stripe_info) {
        return false;
    }

    if (p_stripe_list->num_elem >= SF_MIN_STRIPE_WIDTH-1) {
        return false;
    }

    /* Copy to the next element */
    p_stripe_list->stripe_list[p_stripe_list->num_elem].y_coord = p_stripe_info->y_coord;
    p_stripe_list->stripe_list[p_stripe_list->num_elem].x_coord = p_stripe_info->x_coord;
    p_stripe_list->stripe_list[p_stripe_list->num_elem].x_width = p_stripe_info->x_width;
    ++p_stripe_list->num_elem;

    return true;
}

/* Returns the x coordinate of the first feature that passes selection */
bool sf_find_stripes(sf_gradient_list_t* p_grad_list, sf_stripe_list_t* p_stripe_list) {
    if (!p_grad_list || p_grad_list->num_elem == 0 || !p_stripe_list) {
        return false;
    }

    uint32_t stripe_width = 0;
    uint32_t prev_x = 0;
    size_t i = 0;

    /* No initial stripes */
    p_stripe_list->num_elem = 0;

    /* This assumes the gradient list is sorted by increasing x_coord, which it should be */
    for (i = 0; i < p_grad_list->num_elem; ++i) {
        stripe_width = p_grad_list->gradient_list[i].x_coord - prev_x;
        prev_x = p_grad_list->gradient_list[i].x_coord;

        sf_stripe_info_t stripe_info;
        stripe_info.x_width = stripe_width;
        stripe_info.x_coord = p_grad_list->gradient_list[i].x_coord;
        stripe_info.y_coord = p_grad_list->gradient_list[i].y_coord;

        sf_add_stripe(p_stripe_list, &stripe_info);
    }

    return true;
}

static bool sf_add_feature(sf_feature_list_t* p_feature_list, sf_feature_info_t* p_feature_info) {
    if (!p_feature_list || !p_feature_info) {
        return false;
    }

    if (p_feature_list->num_elem >= SF_MIN_STRIPE_WIDTH-1) {
        return false;
    }

    /* Copy to the next element */
    p_feature_list->feature_list[p_feature_list->num_elem].ratio_error = p_feature_info->ratio_error;
    p_feature_list->feature_list[p_feature_list->num_elem].x_center = p_feature_info->x_center;
    p_feature_list->feature_list[p_feature_list->num_elem].y_center = p_feature_info->y_center;
    p_feature_list->feature_list[p_feature_list->num_elem].x_width = p_feature_info->x_width;
    ++p_feature_list->num_elem;

    return true;
}

bool sf_find_features(sf_stripe_list_t* p_stripe_list, sf_feature_list_t* p_feature_list) {
    if (!p_stripe_list || !p_feature_list) {
        return false;
    }

    /* We are iterating over pairs here, so start at 1 */
    for (size_t i=1; i < p_stripe_list->num_elem; ++i) {
        /* Consecutive stripes have to be larger than the minimum size */
        if (p_stripe_list->stripe_list[i - 1].x_width > SF_MIN_STRIPE_WIDTH &&
            p_stripe_list->stripe_list[i].x_width > SF_MIN_STRIPE_WIDTH) {
            bool b_first_greater = (p_stripe_list->stripe_list[i - 1].x_width > p_stripe_list->stripe_list[i].x_width)
                                   ? true : false;
            float a = (b_first_greater) ? p_stripe_list->stripe_list[i - 1].x_width
                                        : p_stripe_list->stripe_list[i].x_width;
            float b = (b_first_greater) ? p_stripe_list->stripe_list[i].x_width : p_stripe_list->stripe_list[i -
                                                                                                             1].x_width;

            float ab_ratio = a / b;
            float ab_sum_a_ratio = (a + b) / a;

            float ab_ratio_err = fabs(ab_ratio - SF_EXPECTED_RATIO);
            float ab_sum_a_ratio_err = fabs(ab_sum_a_ratio - SF_EXPECTED_RATIO);

            uint32_t x_width = p_stripe_list->stripe_list[i - 1].x_width + p_stripe_list->stripe_list[i].x_width;
            uint32_t x_center = (p_stripe_list->stripe_list[i - 1].x_width + p_stripe_list->stripe_list[i].x_width) / 2;

            /* Compare to target ratio */
            if (ab_ratio_err < SF_RATIO_ALLOWABLE_ERROR && ab_sum_a_ratio_err < SF_RATIO_ALLOWABLE_ERROR) {
                sf_feature_info_t feature_info;
                feature_info.ratio_error = (ab_ratio + ab_sum_a_ratio_err) / 2.0f;
                feature_info.x_width = x_width;
                feature_info.x_center = x_center;
                feature_info.y_center = p_stripe_list->stripe_list[i].y_coord;

                sf_add_feature(p_feature_list, &feature_info);
            }
        }
    }

    return true;
}

static void sf_annotate_features_in_image(int width, int height, uint8_t* p_image_data, uint32_t image_data_len, sf_feature_list_t* p_feature_list) {
    if (!p_image_data || !p_feature_list || p_feature_list->num_elem == 0 || width == 0 || height == 0 || image_data_len == 0) {
        return;
    }

    printf("SF: annotating %u features\n", p_feature_list->num_elem);

    for (size_t i=0; i < p_feature_list->num_elem; ++i) {
        uint32_t x_center = p_feature_list->feature_list[i].x_center;
        uint32_t y_center = p_feature_list->feature_list[i].y_center;

        /* Write inverted pixel at feature center */
        uint8_t pixel_offset = (y_center * width) + x_center;
        p_image_data[pixel_offset] = UINT8_MAX - p_image_data[pixel_offset]; // center
        if (y_center > 0)
            p_image_data[pixel_offset-width] = UINT8_MAX - p_image_data[pixel_offset-width];
        if (y_center < height-1)
            p_image_data[pixel_offset+width] = UINT8_MAX - p_image_data[pixel_offset+width];
        if (x_center < width-1)
            p_image_data[pixel_offset+1] = UINT8_MAX - p_image_data[pixel_offset+1];
        if (x_center > 0)
            p_image_data[pixel_offset-1] = UINT8_MAX - p_image_data[pixel_offset-1];
    }
}

void sf_write_image(const char *p_filename, int width, int height, uint8_t* p_image_data, uint32_t image_data_len,
                    sf_feature_list_t *p_feat_list) {
    if (!p_filename || !p_image_data || image_data_len == 0) {
        return;
    }

    /* Initialize if not already */
    if (stripeFilterColorTable[1].blue == 0) {
        // build color-detect color-table
        for (uint32_t i = 0; i < 256; ++i) {
            stripeFilterColorTable[i].blue = i;
            stripeFilterColorTable[i].green = i;
            stripeFilterColorTable[i].red = i;
            stripeFilterColorTable[1].reserved = 0;
        }
    }

    sf_annotate_features_in_image(width, height, p_image_data, image_data_len, p_feat_list);

    FILE *p_file = fopen(sf_gray_temp_file_name, "w+");

    if (p_file != NULL) {

        // write image
        bmWriteBitmapWithColorTable(p_file, width, height, stripeFilterColorTable, sizeof(stripeFilterColorTable),
                                    p_image_data, image_data_len);

        fflush(p_file);
        fclose(p_file);

        /* Now that write is complete, rename the file */
        rename(sf_gray_temp_file_name, p_filename);
    }
}





