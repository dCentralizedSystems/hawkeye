//
// Created by rsnook on 11/10/20.
//
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "stripe_filter.h"

struct sf_filter;

/* Define a filter-function type */
typedef uint8_t (*sf_filter_fn_t)(struct sf_filter*, uint8_t);

struct sf_filter {
    uint8_t filter_elem[SF_FILTER_NUM_ELEM];
    sf_filter_fn_t filter_fn;
};

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
    if (!p_grad_list || !p_grad_info || p_grad_list->num_elem >= SF_MAX_GRADIENT_LIST_ELEM-1) {
        return false;
    }

    /* Copy to the next element */
    memcpy(&(p_grad_list->gradient_list[++(p_grad_list->num_elem)]), p_grad_info, sizeof(sf_gradient_info_t));

    return true;
}

/* Process a single line of length, len.  Perform a locally-adaptive threshold
 * using a Nx1 horizontal kernel to threshold each pixel.  Look for
 * strong gradients using the adaptive threshold.  Return a list of
 * candidate gradient locations.
 */
bool sf_find_gradients(sf_gradient_list_t* p_grad_list, uint8_t* p_gray, uint32_t len) {
    if (!p_grad_list || !p_gray || !len || len <= SF_FILTER_NUM_ELEM) {
        return false;
    }

    static struct sf_filter filter;
    memset(&filter, 0, sizeof(struct sf_filter));

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
    size_t i=0;
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

                /* Add to gradient list */
                sf_add_gradient(p_grad_list, &grad_info);
            }
        }
        curr_threshold.value = p_gray[i];
    }

    return true;
}

static bool sf_compare_ratio(float a, float b, float expected_ratio, float tolerance) {
    float ab_ratio = a / b;
    float ab_sum_a_ratio = (a+b) / a;

    bool b_ab_ratio = fabs(ab_ratio - expected_ratio) < tolerance;
    bool b_ab_sum_a_ratio = fabs(ab_sum_a_ratio - expected_ratio) < tolerance;

    return b_ab_ratio && b_ab_sum_a_ratio;
}

/* Returns the x coordinate of the first feature that passes selection */
uint32_t sf_process_gradient_list(sf_gradient_list_t* p_grad_list) {
    if (!p_grad_list || p_grad_list->num_elem == 0) {
        return false;
    }
    
    uint32_t dist_from_prev[p_grad_list->num_elem];
    uint32_t prev_x = 0;
    size_t i = 0;
    bool b_found = false;

    memset(dist_from_prev, 0, p_grad_list->num_elem * sizeof(uint32_t));

    /* This assumes the gradient list is sorted by increasing x_coord, which it should be */
    for (i = 0; i < p_grad_list->num_elem; ++i) {
        dist_from_prev[i] = p_grad_list->gradient_list[i].x_coord - prev_x;
        prev_x = p_grad_list->gradient_list[i].x_coord;

        /* Check we aren't on the first feature */
        if (i != 0) {
            bool b_first_greater = (dist_from_prev[i-1] > dist_from_prev[i]) ? true : false;
            float a = (b_first_greater) ? dist_from_prev[i-1] : dist_from_prev[i];
            float b = (b_first_greater) ? dist_from_prev[i] : dist_from_prev[i-1];

            /* Compare to target ratio */
            if (sf_compare_ratio(a, b, SF_EXPECTED_RATIO, SF_RATIO_ALLOWABLE_ERROR)) {
                b_found = true;
                break;
            }
        }
    }

    if (b_found && i >= 2) {
        /* Two-feature region starts at p_grad_list->gradient_list[i-2].x_coord and is dist_from_prev[i-1] +
         * dist_from_prev[i] in length.
         */
        return (p_grad_list->gradient_list[i-2].x_coord + dist_from_prev[i-1] + dist_from_prev[i]) / 2;
    }

    return 0;
 }




