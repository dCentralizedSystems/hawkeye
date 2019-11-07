#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apriltag/apriltag.h"
#include "apriltag/apriltag_pose.h"
#include "apriltag/tagStandard41h12.h"
#include "apriltag/common/pjpeg.h"

#include "apriltag_process.h"

#define MAX_DETECTIONS 			(10)
#define MAX_OUTPUT_STRING_LEN 		(1024)

/**
 * Structure for holding detection information 
 */
typedef struct {
    int nbits;
    int h;
    int id;
    int hamming;
    float decision_margin;
    float pose_error;
} detection_t;

typedef struct {
    size_t num_detections;
    detection_t detections[MAX_DETECTIONS];
} detection_info_t;

char* apriltag_process(int format_in, unsigned int width, unsigned int height, unsigned char* buf, unsigned int buf_len) {
    apriltag_family_t* tf = tagStandard41h12_create();
    apriltag_detector_t* td = apriltag_detector_create();
    pjpeg_t* pjpeg = NULL;
    image_u8x3_t* p_img = NULL;
    int error = 0;

    /**
     * Storage for output string 
     */
    char* p_output = malloc(MAX_OUTPUT_STRING_LEN);
    memset(p_output, 0, MAX_OUTPUT_STRING_LEN);

    /** 
     * Structure for detection information
     */
    detection_info_t det_info_out;
    det_info_out.num_detections = 0;
    
    /* Add tag family in use to detector */
    apriltag_detector_add_family_bits(td, tf, 0);

    pjpeg = pjpeg_create_from_buffer(buf, buf_len, PJPEG_STRICT, &error);

    if (error != PJPEG_OKAY) {
	return "{\"error\" : \"bad_jpeg_convert\"}";
    }

    /* Convert pjpeg opject into image type */
    p_img = pjpeg_to_u8x3_baseline(pjpeg);

    /* Perform detection */
    zarray_t* p_detections = apriltag_detector_detect(td, p_img);

    det_info_out.num_detections = zarray_size(p_detections);

    /* Iterate over detections */
    for (size_t i=0; i < det_info_out.num_detections; ++i) {
	apriltag_detection_t* p_single_det = NULL;
	apriltag_detection_info_t det_info;

	/* Get detection from detections array */
	zarray_get(p_detections, i, &p_single_det);

	det_info.det = p_single_det;

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

	/* Store the detection and pose information */
	det_info_out.detections[i].pose_error = pose_err;
	det_info_out.detections[i].nbits = p_single_det->family->nbits;
	det_info_out.detections[i].h = p_single_det->family->h;
	det_info_out.detections[i].id = p_single_det->id;
	det_info_out.detections[i].hamming = p_single_det->hamming;
	det_info_out.detections[i].decision_margin = p_single_det->decision_margin;
    }

    /* All detection information is in det_info structure, print it out */
    sprintf(p_output, "Apriltags in image: %u {\r\n", det_info_out.num_detections);

    for (size_t i=0; i < det_info_out.num_detections; ++i) {
    	static char fmt[64];
	memset(fmt, 0, 64);

	sprintf(fmt, "  \"id\" : \"%u\",\r\n", det_info_out.detections[i].id);
    	strcat(p_output, fmt);
    }
    strcat(p_output, "}\r\n");

_cleanUp:
    apriltag_detections_destroy(p_detections);
    apriltag_detector_destroy(td);

    if (pjpeg != NULL) {
	pjpeg_destroy(pjpeg);
    }

    if (p_img != NULL) {
	image_u8x3_destroy(p_img);
    }

    return p_output;
}
