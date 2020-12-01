//
// Created by rsnook on 11/10/20.
//

#ifndef _STRIPE_FILTER_H
#define _STRIPE_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define SF_MAX_GRADIENTS                            (100000)
#define SF_MAX_GRADIENT_CLUSTERS                    (1000)
#define SF_MAX_FEATURES                             (100)
#define SF_FILTER_NUM_ELEM                          (3)
#define SF_FILTER_GRAD_THRESHOLD                    (0.30f)
#define SF_MIN_STRIPE_WIDTH                         (10)
#define SF_MAX_STRIPE_WIDTH                         (100)

/* Gradient clustering parameters */
#define SF_CLUSTER_PERCENT_X                        (0.02f)
#define SF_CLUSTER_PERCENT_Y                        (0.3f)
#define SF_CLUSTER_MIN_GRADIENT_COUNT               (10)

/* Expected ratio metrics */
#define SF_EXPECTED_RATIO                           (1.618f)
#define SF_RATIO_ALLOWABLE_ERROR                    (0.15f)

/* Nearest neighbor box parameters for clusters->features */
#define SF_NEAREST_NEIGHBOR_OFF_X                   (SF_MIN_STRIPE_WIDTH)
#define SF_NEAREST_NEIGHBOR_DELTA_X                 (SF_MAX_STRIPE_WIDTH)
#define SF_NEAREST_NEIGHBOR_DELTA_Y                 (7)

typedef enum {
    SF_GRADIENT_NEGATIVE = 0,
    SF_GRADIENT_POSITIVE
} sf_gradient_t;

typedef struct {
    sf_gradient_t type;
    uint32_t x_coord;
    uint32_t y_coord;
    uint8_t value;
} sf_gradient_info_t;

typedef struct {
    int num_elem;
    sf_gradient_info_t gradient_list[SF_MAX_GRADIENTS];
} sf_gradient_list_t;

typedef struct {
    uint32_t x_cent;
    uint32_t y_cent;
    uint32_t count;
    uint32_t x_sum;
    uint32_t y_sum;
    sf_gradient_t type;
} sf_gradient_cluster_info_t;

typedef struct {
    int num_elem;
    sf_gradient_cluster_info_t cluster_list[SF_MAX_GRADIENT_CLUSTERS];
} sf_gradient_cluster_list_t;

typedef struct {
    uint32_t x_min;
    uint32_t x_max;
    uint32_t x_center;
    uint32_t y_center;
    uint32_t x_width;
    float ratio_error;
} sf_feature_info_t;

typedef struct {
    int num_elem;
    sf_feature_info_t feature_list[SF_MAX_FEATURES];
} sf_feature_list_t;

/**
 * @func sf_find_gradients
 * @param p_grad_list List to populate with gradients detected in input image line
 * @param p_gray Pointer to a single horizontal image line in 8-bit grayscale
 * @param len Length in bytes of the image line passed in p_gray
 * @param y_coord The y-coordinate of the line
 * @return True if gradient filter was run successfully, false for invalid parameters
 */
bool sf_find_gradients(sf_gradient_list_t* p_grad_list, uint8_t* p_gray, uint32_t len, uint32_t y_coord);

/**
 * @func sf_cluster_gradients
 * @param p_grad_list The gradients to cluster
 * @param p_cluster_list The list of clusters to check against
 * @return True if clustering was performed successfully, false otherwise
 */
bool sf_cluster_gradients(sf_gradient_list_t* p_grad_list, sf_gradient_cluster_list_t* p_cluster_list);

/**
 * @func sf_find_features
 * @param p_cluster_list The list of gradient clusters detected via @sf_cluster_gradients
 * @param p_feature_list The output list of detetcted features
 * @return True if the feature detection was successful, false if the input parameters were
 * invalid.
 */
bool sf_find_features(sf_gradient_cluster_list_t* p_cluster_list, sf_feature_list_t* p_feature_list);

/**
 * @func sf_write_image
 * @param p_filename c-string filename to write
 * @param width Width of image in pixels
 * @param height Height of the image in pixels
 * @param p_image_data pointer to start of grayscale image data
 * @param image_data_len number of bytes in image
 * @param p_grad_list List of gradients to annotate in the image
 * @param p_cluster_list List of gradient clusters to annotate in the image
 * @param p_feature_list List of features to annotate in the image
 */
void sf_write_image(const char *p_filename, int width, int height, uint8_t *p_image_data, uint32_t image_data_len,
                    sf_gradient_list_t* p_grad_list, sf_gradient_cluster_list_t * p_cluster_list, sf_feature_list_t *p_feat_list);

#endif //_STRIPE_FILTER_H
