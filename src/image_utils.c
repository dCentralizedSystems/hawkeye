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

#include "image_utils.h"
#include "v4l2uvc.h"

#include "logger.h"

/* For detection of apriltags */
#include "apriltag_process.h"

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
    JSAMPROW row_pointer[height];
    static unsigned char *frame_buffer = NULL;
    int z;
    static int written;

    if (frame_buffer == NULL) {
        frame_buffer = calloc(width * 3 * height, 1);
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

    z = 0;
    for (size_t line=0; line < height; ++line) {
	
	    row_pointer[line] = ptr;

        for(size_t x = 0; x < width; x++) {
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
    }

    jpeg_write_scanlines(&cinfo, row_pointer, height);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    //free(frame_buffer);

    return (written);
}

/******************************************************************************
Description.: yuv2jpeg function is based on compress_yuyv_to_jpeg written by
              Gabriel A. Devenyi.
              It uses the destination manager implemented above to compress
              YUYV data to BMP.
Input Value.: video structure from v4l2uvc.c/h, destination buffer and buffersize
              the buffer must be large enough, no error/size checking is done!
Return Value: the buffer will contain the compressed data
******************************************************************************/
size_t compress_yuyv_to_bmp(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height) {
    int z;
    int written = 0;

    unsigned char *ptr = dst;

    z = 0;
    for (size_t line=0; line < height; ++line) {
        for(size_t x = 0; x < width; x++) {
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

            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);

            written += 3;

            if(z++) {
                z = 0;
                src += 4;
            }
        }
    }

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

size_t compress_z16_to_bmp(unsigned char *dst, size_t dst_size, unsigned char* src, size_t src_size, unsigned int width, unsigned int height, int mm_scale) {
    unsigned char *ptr = dst;
    unsigned int written = 0;

    for (uint32_t h=0; h < height; ++h) {
        for(uint32_t w=0; w < width; ++w) {
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
            written += 1;
            src += 2;
        }
    }

    return (written);
}