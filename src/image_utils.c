/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
#   Orginally Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard       #
#   Modifications Copyright (C) 2006  Gabriel A. Devenyi                       #
#   Modifications Copyright (C) 2007  Tom Stöveken                             #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <string.h>

#include "v4l2uvc.h"
#include "color_detect.h"
#include "stripe_filter.h"
#include "image_utils.h"

#define OUTPUT_BUF_SIZE  4096

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    JOCTET * buffer;    /* start of buffer */

    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;

typedef mjpg_destination_mgr * mjpg_dest_ptr;

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) init_destination(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
Description.: called whenever local jpeg buffer fills up
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
Description.: called by jpeg_finish_compress after all data has been written.
              Usually needs to flush buffer.
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) term_destination(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

/******************************************************************************
Description.: Prepare for output to a stdio stream.
Input Value.: buffer is the already allocated buffer memory that will hold
              the compressed picture. "size" is the size in bytes.
Return Value: -
******************************************************************************/
GLOBAL(void) dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size, int *written) {
    mjpg_dest_ptr dest;

    if(cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr) cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = buffer;
    dest->written = written;
}

/******************************************************************************
Description.: yuv2jpeg function is based on compress_yuyv_to_jpeg written by
              Gabriel A. Devenyi.
              It uses the destination manager implemented above to compress
              YUYV data to JPEG. Most other implementations use the
              "jpeg_stdio_dest" from libjpeg, which can not store compressed
              pictures to memory instead of a file.
Input Value.: video structure from v4l2uvc.c/h, destination buffer and buffersize
              the buffer must be large enough, no error/size checking is done!
Return Value: the buffer will contain the compressed data
******************************************************************************/
size_t
compress_yuyv_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char *src, size_t src_size, unsigned int width,
                      unsigned int height, int quality, bool enable_stripe_detect, bool b_write_detect_image) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[height];
    static unsigned char *frame_buffer = NULL;
    static uint8_t* p_gray = NULL;
    static uint8_t* p_gray_image = NULL;
    int z;
    static int written;

    if (frame_buffer == NULL) {
        frame_buffer = calloc(width * 3 * height, 1);
    }

    if (p_gray == NULL) {
        p_gray = calloc(width, 1);
    }

    if (p_gray_image == NULL) {
        p_gray_image = calloc(width * height, 1);
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    dest_buffer(&cinfo, dst, dst_size, &written);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    unsigned char *ptr = frame_buffer;

    uint8_t* p_gray_image_ptr = &p_gray_image[0];

    /* Feature detection lists */
    sf_gradient_list_t grad_list = { 0 };
    sf_gradient_cluster_list_t cluster_list = { 0 };
    sf_feature_list_t feature_list = { 0 };

    z = 0;
    for (size_t line=0; line < height; ++line) {
	    row_pointer[line] = ptr;
	    uint8_t* p_gray_ptr = &p_gray[0];

        for(size_t x = 0; x < width; x++) {
            int r, g, b;
            int y, u, v;

            if(!z)
                y = src[0];
            else
                y = src[2];
            u = src[1] - 128;
            v = src[3] - 128;

            // Use luminance value for gray image
            *p_gray_ptr++ = y;
            *p_gray_image_ptr++ = y;

            r = ((y << 8) + (359 * v)) >> 8;
            g = ((y << 8) - (88 * u) - (183 * v)) >> 8;
            b = ((y << 8) + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if(z++) {
                z = 0;
                src += 4;
            }
        }
        if (enable_stripe_detect) {
            // perform per-line gradient detection
            sf_find_gradients(&grad_list, &p_gray[0], width, line);
        }
    }

    if (enable_stripe_detect) {
        /* Cluster gradients and extract features from gradient clusters */
        sf_cluster_gradients(&grad_list, &cluster_list);
        sf_find_features(&cluster_list, &feature_list);

        if (b_write_detect_image) {
            sf_write_image("./sf_image.bmp", width, height, p_gray_image, width * height, &grad_list, &cluster_list,
                           &feature_list);
        }

        /* Write feature list to JPEG_COM section of image */
        const char *p_feature_string = sf_get_feature_list_data_string(&feature_list);

        if (p_feature_string != NULL) {
            jpeg_write_marker(&cinfo, JPEG_COM, (const JOCTET *) p_feature_string, strlen(p_feature_string));
        }
    }

    jpeg_write_scanlines(&cinfo, row_pointer, height);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    //free(frame_buffer);
    //free(p_gray);

    return (written);
}

#define NUM_DEPTH_PROFILES  (4)
#define PIX_MIN_DISTANCE_MM (20)
#define PIX_MIN_VALUE       (0)
#define PIX_MAX_VALUE       (255)

size_t compress_z16_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality, int mm_scale) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer;
    static int written;

    line_buffer = calloc(width, 1);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* jpeg_stdio_dest (&cinfo, file); */
    dest_buffer(&cinfo, dst, dst_size, &written);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    while(cinfo.next_scanline < height) {
        int x;
        unsigned char *ptr = line_buffer;

        /* Copy input pixels (two bytes each) into output pixels (one byte each) */
        for (x=0; x < width; ++x) {
            unsigned short pix_in = src[0] | (src[1] << 8);
            unsigned char pix_byte = 0;
    
            /* Scale to one byte - scale is set by settings */
            if (mm_scale == 0) {
                pix_in /= PIX_MIN_DISTANCE_MM;
            } else {
                pix_in /= mm_scale;
            }
   
            if (pix_in > PIX_MAX_VALUE)
                pix_in = PIX_MAX_VALUE;

            pix_byte = (unsigned char)pix_in;

            *(ptr++) = pix_byte;
            src += 2;
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return (written);
}

