//
// Created by rsnook on 11/10/20.
//

#ifndef _STRIPE_FILTER_H
#define _STRIPE_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define SF_MAX_GRADIENT_LIST_ELEM                   (200)
#define SF_FILTER_NUM_ELEM                          (4)
#define SF_GRADIENT_MIN_CHANGE                      (5)

/* Expected ratio metrics */
#define SF_EXPECTED_RATIO                           (1.618f)
#define SF_RATIO_ALLOWABLE_ERROR                    (0.1f)

typedef enum {
    SF_GRADIENT_NONE,
    SF_GRADIENT_POSITIVE,
    SG_GRADIENT_NEGATIVE
} sf_gradient_t;

typedef enum {
    SF_THRESHOLD_STATE_ABOVE,
    SF_THRESHOLD_STATE_BELOW
} sf_threshold_state_t;

typedef struct {
    sf_threshold_state_t state;
    uint8_t value;
} sf_threshold_t;

typedef struct {
    sf_gradient_t type;
    uint32_t x_coord;
    uint8_t value;
} sf_gradient_info_t;

typedef struct {
    uint32_t x_coord_start;
    uint32_t x_coord_end;
    sf_threshold_state_t region_type;
} sf_region_info_t;

typedef struct {
    int num_elem;
    sf_gradient_info_t gradient_list[SF_MAX_GRADIENT_LIST_ELEM];
} sf_gradient_list_t;

bool sf_find_gradients(sf_gradient_list_t* p_grad_list, uint8_t* p_gray, uint32_t len);

#endif //_STRIPE_FILTER_H
