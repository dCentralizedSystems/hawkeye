#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apriltag/apriltag.h"
#include "apriltag/apriltag_pose.h"
#include "apriltag/tagStandard41h12.h"
#include "apriltag/common/pjpeg.h"

#include "apriltag_process.h"

#define MAX_DETECTIONS          (10)
#define MAX_OUTPUT_STRING_LEN       (1024)

char* apriltag_process(image_u8_t* p_img) {
    static apriltag_family_t* tf = NULL;
    static apriltag_detector_t* td = NULL;
    static char* p_output = NULL;

    if (tf == NULL) {
        tf = tagStandard41h12_create();
    }

    if (td == NULL) {
        td = apriltag_detector_create();

        /* Add tag family in use to detector */
        apriltag_detector_add_family_bits(td, tf, 0);
    }

    /**
     * Storage for output string 
     */
    if (p_output == NULL) {
        p_output = calloc(MAX_OUTPUT_STRING_LEN, 1);
    }

    memset(p_output, 0, MAX_OUTPUT_STRING_LEN);

    /* Perform detection */
    zarray_t* p_detections = apriltag_detector_detect(td, p_img);

    size_t det_count = zarray_size(p_detections);
    sprintf(p_output, "{\n  \"tags\" : \"%u\",\n", det_count);

    /* Iterate over detections */
    for (size_t i=0; i < det_count; ++i) {
        static char fmt[64];

        apriltag_detection_t* p_det = NULL;

        /* Get detection from detections array */
        zarray_get(p_detections, i, &p_det);

#ifdef POSE_DETECT 
        apriltag_detection_info_t det_info;
        det_info.det = p_det;

        /* Configure detection info for pose estimation */
        /* For RealSense D435, using the OV2740 sensor:
         *   fx = 1.88mm / 1.4um = 1343 
         *   fy = 1.88mm / 1.4um = 1343
         *   cx = width / 2
         *   cy = height / 2
         *   
         *   Hardcoded tag size = 0.1016m
         */
        det_info.fx = 1343;
        det_info.fy = 1343;
        det_info.cx = width / 2;
        det_info.cy = height / 2;
        det_info.tagsize = 0.1016;

        apriltag_pose_t pose;
        double pose_err = estimate_tag_pose(&det_info, &pose);
#endif /* #ifdef POSE_DETECT */

        memset(fmt, 0, 64);
        sprintf(fmt, "  {\n    \"id\" : \"%u\",\n", p_det->id);
        strcat(p_output, fmt);
        for (size_t c=0; c < 4; ++c) {
            memset(fmt, 0, 64);
            sprintf(fmt, "    \"coord[%u]\" : \"%u,%u\",\n", c, (unsigned)p_det->p[c][0], (unsigned)p_det->p[c][1]);
            strcat(p_output, fmt);
        }
        strcat(p_output, "  },\n");

//#define TAG_DETECT_LINE
#ifdef TAG_DETECT_LINE
        for (size_t i=0; i < 3; ++i) {
            image_u8_draw_line(p_img, p_det->p[i][0], p_det->p[i][1], p_det->p[i+1][0], p_det->p[i+1][1], 255, 1); 
        }
        image_u8_draw_line(p_img, p_det->p[3][0], p_det->p[3][1], p_det->p[0][0], p_det->p[0][1], 255, 1); 
        image_u8_write_pnm(p_img, "tags.pnm");
#endif
    }

    strcat(p_output, "}\n");

#ifdef PRINT_DETECTIONS
    printf("%s:\n%s\n", __func__, p_output);
#endif

    apriltag_detections_destroy(p_detections);
    //apriltag_detector_destroy(td);

    if (p_img != NULL) {
        //image_u8_destroy(p_img);
    }

    return p_output;
}
