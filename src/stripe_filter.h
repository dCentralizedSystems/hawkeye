//
// Created by rsnook on 11/10/20.
//

#ifndef _STRIPE_FILTER_H
#define _STRIPE_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define SF_MAX_FEATURES                             (1000)
#define SF_FILTER_NUM_ELEM                          (4)
#define SF_GRADIENT_MIN_CHANGE                      (30)
#define SF_MIN_STRIPE_WIDTH                         (10)

/* Expected ratio metrics */
#define SF_EXPECTED_RATIO                           (1.618f)
#define SF_RATIO_ALLOWABLE_ERROR                    (0.1f)

typedef enum {
    SF_GRADIENT_NONE,
    SF_GRADIENT_POSITIVE,
    SG_GRADIENT_NEGATIVE
} sf_gradient_t;

typedef struct {
    sf_gradient_t type;
    uint32_t x_coord;
    uint32_t y_coord;
    uint8_t value;
} sf_gradient_info_t;

typedef struct {
    int num_elem;
    sf_gradient_info_t gradient_list[SF_MAX_FEATURES];
} sf_gradient_list_t;

typedef struct {
    uint32_t x_coord;
    uint32_t x_width;
    uint32_t y_coord;
} sf_stripe_info_t;

typedef struct {
    int num_elem;
    sf_stripe_info_t stripe_list[SF_MAX_FEATURES];
} sf_stripe_list_t;

typedef struct {
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
 * @func sf_find_stripes
 * @param p_grad_list The gradient list returned from a successful invocation of @sf_find_gradients
 * @param p_stripe_list The list of detected stripes
 * @return True if the stripe detection was performed successfully (regardless of the nubmer of detections),
 * false if there was a problem with the arguments.
 * features were detected.
 */
bool sf_find_stripes(sf_gradient_list_t* p_grad_list, sf_stripe_list_t* p_stripe_list);

/**
 * @func sf_find_features
 * @param p_stripe_list The list of stripes detected via @sf_find_stripes
 * @param p_feature_list The output list of detetcted features
 * @return True if the feature detection was successful, false if the input parameters were
 * invalid.
 */
bool sf_find_features(sf_stripe_list_t* p_stripe_list, sf_feature_list_t* p_feature_list);


/**
 * @func sf_write_image
 * @param p_filename c-string filename to write
 * @param width Width of image in pixels
 * @param height Height of the image in pixels
 * @param p_image_data pointer to start of grayscale image data
 * @param image_data_len number of bytes in image
 * @param p_feature_list List of features to annotate in the image
 */
void sf_write_image(const char *p_filename, int width, int height, uint8_t *p_image_data, uint32_t image_data_len,
                    sf_feature_list_t *p_feat_list);

#endif //_STRIPE_FILTER_H
