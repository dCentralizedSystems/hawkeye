//
// Created by rsnook on 11/10/20.
//
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "bitmap.h"
#include "stripe_filter.h"

#define SF_GRADIENT_POSITIVE_ANNOTATION_COLOR   (0)
#define SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR   (1)
#define SF_GRADIENT_CLUSTER_ANNOTATION_COLOR    (2)
#define SF_FEATURE_ANNOTATION_COLOR             (3)
#define SF_FEATURE_CENTER_ANNOTATION_COLOR      (4)

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

    float accum = 0;

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

    if (p_grad_list->num_elem >= SF_MAX_GRADIENTS-1) {
        return false;
    }

    /* Copy to the current element */
    p_grad_list->gradient_list[p_grad_list->num_elem] = *p_grad_info;
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

    /* Start at filter-width from the left image edge,
     * populate the filter with the initial SF_FILTER_NUM_ELEM pixels
     */
    size_t i;
    for (i=0; i < SF_FILTER_NUM_ELEM; ++i) {
        filter.filter_fn(&filter, p_gray[i]);
    }

    /* Iterate over line (staring from filter width) */
    for (i=SF_FILTER_NUM_ELEM;i < len; ++i) {
        /* Calculate the threshold value at the current pixel and add the current pixel value to the filter */
        uint8_t filter_value = filter.filter_fn(&filter, p_gray[i]);
        uint8_t threshold_level = (SF_FILTER_GRAD_THRESHOLD * (float)filter_value);
        uint16_t filter_max = filter_value + threshold_level;
        uint16_t filter_min = (filter_value < threshold_level) ? 0 : filter_value - threshold_level;

        /* Check for gradients */
        if (p_gray[i] > filter_max ) {
            /* Above threshold */
            if (curr_threshold.state == SF_THRESHOLD_STATE_BELOW) {
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
            if (curr_threshold.state == SF_THRESHOLD_STATE_ABOVE) {
                /* Negative gradient */
                curr_threshold.state = SF_THRESHOLD_STATE_BELOW;

                /* Build information about gradient */
                grad_info.value = p_gray[i];
                grad_info.type = SF_GRADIENT_NEGATIVE;
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

/* Inclusive of end-points */
static bool sf_value_in_range(int32_t base, float range, int32_t value) {
    int32_t range_delta = (int32_t)((float)base * range);

    /* Simple detect bounds */
    if (value < base - range_delta || value > base + range_delta) {
        return false;
    }

    return true;
}

static bool sf_cluster_grad(sf_gradient_info_t* p_grad, sf_gradient_cluster_list_t* p_cluster_list) {
    if (!p_grad || !p_cluster_list) {
        return false;
    }

    for (size_t i=0; i < p_cluster_list->num_elem; ++i) {
        /* The current cluster we're working with */
        sf_gradient_cluster_info_t* p_cluster = &p_cluster_list->cluster_list[i];

        /* If the current gradient is within the clustering percentage of the current
         * cluster centroid, and of the same type, add it to the cluster.
         */
        if (sf_value_in_range(p_cluster->x_cent, SF_CLUSTER_PERCENT_X, p_grad->x_coord) &&
            sf_value_in_range(p_cluster->y_cent, SF_CLUSTER_PERCENT_Y, p_grad->y_coord) &&
            p_grad->type == p_cluster->type) {

            /* Add to current cluster */
            p_cluster->x_sum += p_grad->x_coord;
            p_cluster->y_sum += p_grad->y_coord;
            p_cluster->count++;
            p_cluster->x_cent = p_cluster->x_sum / p_cluster->count;
            p_cluster->y_cent = p_cluster->y_sum / p_cluster->count;
            return true;
        }
    }
    return false;
}

static bool sf_create_cluster_from_grad(sf_gradient_info_t* p_grad, sf_gradient_cluster_list_t* p_cluster_list) {
    if (!p_grad || !p_cluster_list) {
        return false;
    }

    /* Cluster list full */
    if (p_cluster_list->num_elem >= SF_MAX_GRADIENT_CLUSTERS-1) {
        return false;
    }

    /* Add gradient to cluster */
    p_cluster_list->cluster_list[p_cluster_list->num_elem].count = 1;
    p_cluster_list->cluster_list[p_cluster_list->num_elem].x_cent = p_grad->x_coord;
    p_cluster_list->cluster_list[p_cluster_list->num_elem].y_cent = p_grad->y_coord;
    p_cluster_list->cluster_list[p_cluster_list->num_elem].x_sum = p_grad->x_coord;
    p_cluster_list->cluster_list[p_cluster_list->num_elem].y_sum = p_grad->y_coord;
    p_cluster_list->cluster_list[p_cluster_list->num_elem].type = p_grad->type;
    p_cluster_list->num_elem++;

    /* Cluster was added */
    return true;
}

bool sf_cluster_gradients(sf_gradient_list_t* p_grad, sf_gradient_cluster_list_t* p_cluster_list) {
    if (!p_grad || p_grad->num_elem == 0 || !p_cluster_list) {
        return false;
    }

    /**
     * Iterate over gradients and determine whether to add the current gradient to an existing
     * cluster or create a new cluster.
     */
    for (size_t i=0; i < p_grad->num_elem; ++i) {
        sf_gradient_info_t* p_cur_grad = &p_grad->gradient_list[i];

        if (!sf_cluster_grad(p_cur_grad, p_cluster_list)) {
           /* Didn't glob on to a cluster, create a new one */
           sf_create_cluster_from_grad(p_cur_grad, p_cluster_list);
        }
    }
    return true;
}

static bool sf_add_feature(sf_feature_list_t* p_feature_list, sf_feature_info_t* p_feature_info) {
    if (!p_feature_list || !p_feature_info) {
        return false;
    }

    if (p_feature_list->num_elem >= SF_MAX_FEATURES-1) {
        return false;
    }

    /* Copy to the current element */
    p_feature_list->feature_list[p_feature_list->num_elem] = *p_feature_info;
    ++p_feature_list->num_elem;

    return true;
}

static size_t sf_find_nearest_neighbor_in_box(sf_gradient_cluster_list_t* p_cluster_list, size_t base_cluster_index, uint32_t x_off, uint32_t delta_x, uint32_t delta_y) {
    if (!p_cluster_list || p_cluster_list->num_elem < base_cluster_index) {
        return 0;
    }
    /* The cluster to start from */
    sf_gradient_cluster_info_t* p_base = &p_cluster_list->cluster_list[base_cluster_index];
    uint32_t min_x = UINT32_MAX;
    size_t min_index = 0;

    for (size_t i=0; i < p_cluster_list->num_elem; ++i) {
        if (i != base_cluster_index) {
            sf_gradient_cluster_info_t* p_cur = &p_cluster_list->cluster_list[i];

            /* Check if this cluster is inside the bounding box (x+off_x, y-delta_y), (x+off_x+delta_x, y+delta_y) */
            if (p_cur->x_cent <= p_base->x_cent + x_off || p_cur->x_cent > p_base->x_cent + x_off + delta_x) {
                continue;
            }

            uint32_t top_box_y = (p_base->y_cent < delta_y) ? 0 : p_base->y_cent - delta_y;
            if (p_cur->y_cent <= top_box_y || p_cur->y_cent >= p_base->y_cent + delta_y) {
                continue;
            }

            /* The current (p_cur) cluster is within the bounding box, check if it's the closest */
            if (p_cur->x_cent - p_base->x_cent < min_x) {
                /* This is the closest */
                min_x = p_cur->x_cent - p_base->x_cent;
                min_index = i;
            }
        }
    }

    return min_index;
}

bool sf_find_features(sf_gradient_cluster_list_t* p_cluster_list, sf_feature_list_t* p_feature_list) {
    if (!p_cluster_list || p_cluster_list->num_elem == 0 || !p_feature_list) {
        return false;
    }

    /* Iterate over gradient clusters */
    for (size_t i=0; i < p_cluster_list->num_elem; ++i) {
        /* Capture the three clusters to use for comparison */
        sf_gradient_cluster_info_t* p_cluster1 = &p_cluster_list->cluster_list[i];

        size_t cluster2_index = sf_find_nearest_neighbor_in_box(p_cluster_list,
                                                                i,
                                                                SF_NEAREST_NEIGHBOR_OFF_X,
                                                                SF_NEAREST_NEIGHBOR_DELTA_X,
                                                                SF_NEAREST_NEIGHBOR_DELTA_Y);
        /* No matching nearest neighbor */
        if (cluster2_index == 0) {
            continue;
        }

        sf_gradient_cluster_info_t* p_cluster2 = &p_cluster_list->cluster_list[cluster2_index];

        size_t cluster3_index = sf_find_nearest_neighbor_in_box(p_cluster_list,
                                                                cluster2_index,
                                                                SF_NEAREST_NEIGHBOR_OFF_X,
                                                                SF_NEAREST_NEIGHBOR_DELTA_X,
                                                                SF_NEAREST_NEIGHBOR_DELTA_Y);
        /* No matching nearest neighbor */
        if (cluster3_index == 0) {
            continue;
        }
        sf_gradient_cluster_info_t* p_cluster3 = &p_cluster_list->cluster_list[cluster3_index];

        /* Ignore too-small clusters, move index so that we don't use it in the future */
        if (p_cluster1->count < SF_CLUSTER_MIN_GRADIENT_COUNT ||
            p_cluster2->count < SF_CLUSTER_MIN_GRADIENT_COUNT ||
            p_cluster3->count < SF_CLUSTER_MIN_GRADIENT_COUNT) {
            continue;
        }

        int32_t x_width21 = p_cluster2->x_cent - p_cluster1->x_cent;
        int32_t x_width32 = p_cluster3->x_cent - p_cluster2->x_cent;

        if (x_width21 < 0 ||
            x_width32 < 0 ||
            x_width21 < SF_MIN_STRIPE_WIDTH ||
            x_width21 > SF_MAX_STRIPE_WIDTH ||
            x_width32 < SF_MIN_STRIPE_WIDTH ||
            x_width32 > SF_MAX_STRIPE_WIDTH) {
            /* Invalid ordering or stripe size violation */
            continue;
        }
        bool b_first_greater = (x_width21 > x_width32) ? true : false;
        float a = (b_first_greater) ? x_width21 : x_width32;
        float b = (b_first_greater) ? x_width32 : x_width21;

        float ab_ratio = a / b;
        float ab_sum_a_ratio = (a + b) / a;
        float ab_ratio_err = fabs(ab_ratio - SF_EXPECTED_RATIO);
        float ab_sum_a_ratio_err = fabs(ab_sum_a_ratio - SF_EXPECTED_RATIO);

        uint32_t x_width = x_width21 + x_width32;
        uint32_t x_center = p_cluster1->x_cent + (x_width / 2);

        /* Compare to target ratio */
        if (ab_ratio_err < SF_RATIO_ALLOWABLE_ERROR && ab_sum_a_ratio_err < SF_RATIO_ALLOWABLE_ERROR) {
            sf_feature_info_t feature_info;
            feature_info.ratio_error = (ab_ratio_err + ab_sum_a_ratio_err) / 2.0f;
            feature_info.x_width = x_width;
            feature_info.x_center = x_center;
            feature_info.y_center = p_cluster2->y_cent;
            feature_info.x_min = p_cluster1->x_cent;
            feature_info.x_max = p_cluster3->x_cent;

            sf_add_feature(p_feature_list, &feature_info);
        }
    }
    return true;
}


static void sf_annotate_gradients_in_image(int width, int height, uint8_t* p_image_data, uint32_t image_data_len, sf_gradient_list_t* p_grad_list) {
    if (!p_image_data || !p_grad_list || p_grad_list->num_elem == 0 || width == 0 || height == 0 || image_data_len == 0) {
        return;
    }

    printf("SF: annotating %u gradients\n", p_grad_list->num_elem);

    for (size_t i=0; i < p_grad_list->num_elem; ++i) {
        uint32_t x_center = p_grad_list->gradient_list[i].x_coord;
        uint32_t y_center = p_grad_list->gradient_list[i].y_coord;
        sf_gradient_t type = p_grad_list->gradient_list[i].type;
        uint8_t grad_color;

        grad_color = (type == SF_GRADIENT_POSITIVE) ? SF_GRADIENT_POSITIVE_ANNOTATION_COLOR : SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR;

        /* Write inverted pixel at feature center */
        uint32_t pixel_offset = (y_center * width) + x_center;
        p_image_data[pixel_offset] = grad_color; // center
        if (y_center > 0)
            p_image_data[pixel_offset-width] = grad_color;
        if (y_center < height-1)
            p_image_data[pixel_offset+width] = grad_color;
    }
}

static void sf_annotate_clusters_in_image(int width, int height, uint8_t* p_image_data, uint32_t image_data_len, sf_gradient_cluster_list_t* p_cluster_list) {
    if (!p_image_data || !p_cluster_list || p_cluster_list->num_elem == 0 || width == 0 || height == 0 || image_data_len == 0) {
        return;
    }

    printf("SF: annotating %u clusters\n", p_cluster_list->num_elem);

    for (size_t i=0; i < p_cluster_list->num_elem; ++i) {
        uint32_t x_left = p_cluster_list->cluster_list[i].x_cent;
        uint32_t y_center = p_cluster_list->cluster_list[i].y_cent;
        uint32_t start_offset = (y_center * width) + x_left;

        /* Write dot for cluster location */
        p_image_data[start_offset] = SF_GRADIENT_CLUSTER_ANNOTATION_COLOR;
    }
}

static void sf_annotate_features_in_image(int width, int height, uint8_t* p_image_data, uint32_t image_data_len, sf_feature_list_t* p_feature_list) {
    if (!p_image_data || !p_feature_list || p_feature_list->num_elem == 0 || width == 0 || height == 0 || image_data_len == 0) {
        return;
    }

    printf("SF: annotating %u features\n", p_feature_list->num_elem);

    for (size_t i=0; i < p_feature_list->num_elem; ++i) {
        uint32_t x_min = p_feature_list->feature_list[i].x_min;
        uint32_t x_width = p_feature_list->feature_list[i].x_width;
        uint32_t y_center = p_feature_list->feature_list[i].y_center;
        uint32_t start_offset = (y_center * width) + x_min;

        /* Write line for feature width */
        for (size_t i=start_offset; i < start_offset + x_width; ++i) {
            p_image_data[i] = SF_FEATURE_ANNOTATION_COLOR;
        }

        /* Mark feature center */
        p_image_data[start_offset + (x_width / 2)] = SF_FEATURE_CENTER_ANNOTATION_COLOR;
    }
}

void sf_write_image(const char *p_filename, int width, int height, uint8_t* p_image_data, uint32_t image_data_len,
                    sf_gradient_list_t* p_grad_list, sf_gradient_cluster_list_t* p_cluster_list, sf_feature_list_t *p_feat_list) {
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

        /* Set up custom colors */
        /* Gradient positive annotation */
        stripeFilterColorTable[SF_GRADIENT_POSITIVE_ANNOTATION_COLOR].blue = 0;
        stripeFilterColorTable[SF_GRADIENT_POSITIVE_ANNOTATION_COLOR].green = 240;
        stripeFilterColorTable[SF_GRADIENT_POSITIVE_ANNOTATION_COLOR].red = 0;
        stripeFilterColorTable[SF_GRADIENT_POSITIVE_ANNOTATION_COLOR].reserved = 0;

        /* Gradient negative annotation */
        stripeFilterColorTable[SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR].blue = 0;
        stripeFilterColorTable[SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR].green = 0;
        stripeFilterColorTable[SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR].red = 240;
        stripeFilterColorTable[SF_GRADIENT_NEGATIVE_ANNOTATION_COLOR].reserved = 0;

        /* Stripe annotation */
        stripeFilterColorTable[SF_GRADIENT_CLUSTER_ANNOTATION_COLOR].blue = 240;
        stripeFilterColorTable[SF_GRADIENT_CLUSTER_ANNOTATION_COLOR].green = 80;
        stripeFilterColorTable[SF_GRADIENT_CLUSTER_ANNOTATION_COLOR].red = 120;
        stripeFilterColorTable[SF_GRADIENT_CLUSTER_ANNOTATION_COLOR].reserved = 0;

        /* Feature annotation */
        stripeFilterColorTable[SF_FEATURE_ANNOTATION_COLOR].blue = 240;
        stripeFilterColorTable[SF_FEATURE_ANNOTATION_COLOR].green = 240;
        stripeFilterColorTable[SF_FEATURE_ANNOTATION_COLOR].red = 60;
        stripeFilterColorTable[SF_FEATURE_ANNOTATION_COLOR].reserved = 0;

        /* Feature center annotation */
        stripeFilterColorTable[SF_FEATURE_CENTER_ANNOTATION_COLOR].blue = 0;
        stripeFilterColorTable[SF_FEATURE_CENTER_ANNOTATION_COLOR].green = 0;
        stripeFilterColorTable[SF_FEATURE_CENTER_ANNOTATION_COLOR].red = 0;
        stripeFilterColorTable[SF_FEATURE_CENTER_ANNOTATION_COLOR].reserved = 0;
    }

    sf_annotate_gradients_in_image(width, height, p_image_data, image_data_len, p_grad_list);
    sf_annotate_clusters_in_image(width, height, p_image_data, image_data_len, p_cluster_list);
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

const char* sf_get_feature_list_data_string(sf_feature_list_t* p_feature_list) {
    static char feature_list_string[FEATURE_LIST_STRING_MAX_LENGTH];

    memset(feature_list_string, 0, FEATURE_LIST_STRING_MAX_LENGTH);

    if (!p_feature_list) {
        return NULL;
    }

    // only write complete blobs, check if there are any to write
    int num_features = p_feature_list->num_elem;

    if (num_features == 0) {
        return NULL;
    }

    // keep track of remaining space in first image line
    int feature_list_string_remaining = FEATURE_LIST_STRING_MAX_LENGTH;

    // write num blobs first
    int num_blob_string_len = snprintf(feature_list_string, feature_list_string_remaining, "%d", num_features);

    feature_list_string_remaining -= num_blob_string_len;

    // add each complete and valid blob to string
    for (size_t i = 0; i < num_features; ++i) {
        sf_feature_info_t* p_feat = &p_feature_list->feature_list[i];

        char feat_string[128] = { 0 }; // 10 characters per integer plus 6 commas plus null terminator

        int feat_string_len = snprintf(feat_string, 128, ",%d,%d,%d,%d,%d,%d,%d", (uint32_t)i, p_feat->x_min, p_feat->x_max,
                                       p_feat->y_center, p_feat->y_center, 1, p_feat->x_max - p_feat->x_min);

        // ensure room to append in p_line_buf
        if (feature_list_string_remaining < feat_string_len) {
            break;
        }

        // write blob string
        strcat(feature_list_string, feat_string);

        feature_list_string_remaining -= feat_string_len;
    }

    if (feature_list_string_remaining > 0) {
        // append a line-feed
        strcat(feature_list_string, "\n");
    }

    return feature_list_string;
}





