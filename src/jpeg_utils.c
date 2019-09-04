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
#include <math.h>

#include "jpeg_utils.h"
#include "v4l2uvc.h"

#include "logger.h"

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
size_t compress_yuyv_to_jpeg(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer;
    int z;
    static int written;

    line_buffer = calloc(width * 3, 1);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* jpeg_stdio_dest (&cinfo, file); */
    dest_buffer(&cinfo, dst, dst_size, &written);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    z = 0;
    while(cinfo.next_scanline < height) {
        int x;
        unsigned char *ptr = line_buffer;

        for(x = 0; x < width; x++) {
            int r, g, b;
            int y, u, v;

            if(!z)
                y = src[0] << 8;
            else
                y = src[2] << 8;
            u = src[1] - 128;
            v = src[3] - 128;

            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if(z++) {
                z = 0;
                src += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return (written);
}

#define NUM_DEPTH_PROFILES (4)
#define PIX_MIN_DISTANCE_MM (20)
#define PIX_MIN_VALUE       (0)
#define PIX_MAX_VALUE       (255)

size_t compress_z16_to_jpeg_and_depth_profile(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int quality) {
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

    /* allocate storage for depth profile line and set to zero */
    unsigned char *depth_profiles = calloc(width, NUM_DEPTH_PROFILES);

    if (depth_profiles == NULL) {
        log_itf(LOG_ERROR, "%s: can't allocate depth profile buffers", __func__);
        return 0;
    }

    /* Determine how many lines per depth profile */
    size_t lines_per_depth_profile = (height-NUM_DEPTH_PROFILES) / NUM_DEPTH_PROFILES;

    while(cinfo.next_scanline < height) {
        int x;
        unsigned char *ptr = line_buffer;
        size_t depth_profile_index = cinfo.next_scanline / lines_per_depth_profile;

        /* There might be extra lines, due to the integer math, stick these in the last depth profile */
        if (depth_profile_index >= NUM_DEPTH_PROFILES) {
            depth_profile_index = NUM_DEPTH_PROFILES-1;
        } 

        size_t start_index = depth_profile_index * width;
        
        /* Check for writing depth profiles */
        if (cinfo.next_scanline >= height - NUM_DEPTH_PROFILES) {
            row_pointer[0] = &depth_profiles[width * (cinfo.next_scanline - (height - NUM_DEPTH_PROFILES))];
        } else {
            /* Copy input pixels (two bytes each) into output pixels (one byte each) */
            for (x=0; x < width; ++x) {
                unsigned short pix_in = src[0] | (src[1] << 8);
                unsigned char pix_byte = 0;
    
                /* Scale to one byte */
                pix_in /= PIX_MIN_DISTANCE_MM;
    
                if (pix_in > PIX_MAX_VALUE)
                    pix_in = PIX_MAX_VALUE;

                pix_byte = (unsigned char)pix_in;

                /* Update minima */
                if (pix_byte > PIX_MIN_DISTANCE_MM) {
                    /* Smallest pixel value greater than 20 */
                    if (depth_profiles[start_index + x] == PIX_MIN_VALUE && pix_byte < PIX_MAX_VALUE) {
                        depth_profiles[start_index + x] = pix_byte;
                    }    
                    depth_profiles[start_index + x] = (pix_byte < depth_profiles[start_index + x]) ? pix_byte : depth_profiles[start_index + x];
                }
    
                *(ptr++) = pix_byte;
                src += 2;
            }

            row_pointer[0] = line_buffer;
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);
    free(depth_profiles);

    return (written);
}

void ground_plane_filter(unsigned char *depth_img, int width, size_t src_size, float view_height, float h_fov, float v_fov, float pitch, float err_thresh) {
    int height = src_size / width;
    for(int i = 0; i < src_size - 1; i += 2) {
        int row = i / width;
        int col = i % width;

        float theta = fabs((((col + 1.0) / width - 0.5) * h_fov) * M_PI / 180);
        float phi = ((row + 1.0) / height - 0.5) * v_fov * M_PI / 180;
        phi += pitch;

        float ground_depth = view_height / sin(phi) / cos(theta);
        unsigned short obs_depth = depth_img[i] | (depth_img[i + 1] << 8);

        // in z16 every 2 bytes (pixels) represents a depth value
        if (phi > 0 && obs_depth >= ground_depth - err_thresh) {
            depth_img[i] = PIX_MAX_VALUE;
            depth_img[i + 1] = PIX_MAX_VALUE;
        }
    }
}


void ground_plane_filter_high_depth_inversion(unsigned char *depth_img, int width, size_t src_size, float view_height, float h_fov, float v_fov, float pitch, float err_thresh) {
    int height = src_size / width;
    for (int i = 0; i < src_size; i += 2) {
        int row = i / width;
        int col = i % width;

        float theta = fabs((((col + 1.0) / width - 0.5) * h_fov) * M_PI / 180);
        float phi = ((row + 1.0) / height - 0.5) * v_fov * M_PI / 180;
        phi += pitch;

        float ground_depth = view_height / sin(phi) / cos(theta);
        unsigned short obs_depth = depth_img[i] | (depth_img[i + 1] << 8);

        float delta_depth = obs_depth - ground_depth;

        if (phi > 0 && fabs(delta_depth) <= err_thresh) {
            depth_img[i] = PIX_MAX_VALUE;
            depth_img[i + 1] = PIX_MAX_VALUE;
        } else if (phi > 0 && delta_depth > err_thresh) {
            unsigned short inv_depth = (unsigned short) (ground_depth - err_thresh);
            depth_img[i] = (unsigned char) (inv_depth & 0xff00);
            depth_img[i + 1] = inv_depth >> 8; 
        }
    }
}

