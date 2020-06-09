
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>

#include "memory.h"
#include "frames.h"
#include "v4l2uvc.h"
#include "image_utils.h"
#include "color_detect.h"
#include "bitmap.h"
#include "server.h"
#include "utils.h"
#include "daemon.h"
#include "settings.h"

#define FRAME_BUFFER_LENGTH 8

static int is_running = 1;

typedef enum {
    OUTPUT_FILE_TYPE_NONE,
    OUTPUT_FILE_TYPE_JPG,
    OUTPUT_FILE_TYPE_BMP
} output_file_t;


const char *p_color_detect_file_name = "detect_color_image.bmp~";
const char *p_color_detect_file_rename = "detect_color_image.bmp";

static void signal_handler(int sig){
    switch (sig) {
        case SIGINT:
            is_running = 0;
            break;
        case SIGTERM:
            is_running = 0;
            break;
    }
}

void init_signals(){
    struct sigaction sigact;

    sigact.sa_handler = signal_handler;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigaction(SIGINT, &sigact, (struct sigaction *) NULL);
    sigaction(SIGTERM, &sigact, (struct sigaction *) NULL);
    sigaction(SIGHUP, &sigact, (struct sigaction *) NULL);
    sigaction(SIGUSR1, &sigact, (struct sigaction *) NULL);
    sigaction(SIGUSR2, &sigact, (struct sigaction *) NULL);

    signal(SIGPIPE, SIG_IGN);
}

// Not used currently, but may use later to gather statistics
/*
double get_real_fps(struct video_device *vd, unsigned int rounds) {
    struct timeval start, end;
    unsigned int i;
    double total = 0;

    // Do two grabs first since the first time we run initialization
    capture_frame(vd);
    capture_frame(vd);

    for (i = 0; i < rounds; i++) {
        gettimeofday(&start, NULL);
        capture_frame(vd);
        gettimeofday(&end, NULL);
        total += (double) (end.tv_sec - start.tv_sec) + ((double) (end.tv_usec - start.tv_usec)) / 10000000;
    }

    return rounds/total;
}
*/

struct frame_buffers *init_frame_buffers(size_t device_count, char *device_name) {
    int i;
    struct frame_buffer *fb;
    struct frame_buffers *fbs;

    fbs = malloc(sizeof(struct frame_buffers));
    fbs->count = 0;
    fbs->buffers = calloc(device_count, sizeof(struct frame_buffer));

    for (i = 0; i < device_count; i++) {
        fb = &fbs->buffers[i];

        create_frame_buffer(fb, FRAME_BUFFER_LENGTH);
        if ((fb->vd = create_video_device(device_name, settings.width, settings.height, settings.fps, settings.v4l2_format, settings.jpeg_quality)) == NULL) {
            user_panic("Could not initialize video device.");
        }

        fbs->count++;
    }

    return fbs;
}

void destroy_frame_buffers(struct frame_buffers *fbs) {
    int i;
    struct frame_buffer *fb;

    for (i = 0; i < fbs->count; i++) {
        fb = &fbs->buffers[i];

        destroy_video_device(fb->vd);
        destroy_frame_buffer(fb);
    }

    free(fbs->buffers);
    free(fbs);
}

void write_frame(struct frame_buffer *fb, void *data, size_t data_len, output_file_t file_type) {

    static char out_file_path[128] = {0};
    static char temp_out_file_path[128] = {0};

    if (out_file_path[0] == 0 && temp_out_file_path[0] == 0) {
        if (file_type == OUTPUT_FILE_TYPE_BMP) {
            sprintf(temp_out_file_path, "%s/%s.bmp~", settings.file_root, settings.base_file_name);
            sprintf(out_file_path, "%s/%s.bmp", settings.file_root, settings.base_file_name);
        } else {
            sprintf(temp_out_file_path, "%s/%s.jpg~", settings.file_root, settings.base_file_name);
            sprintf(out_file_path, "%s/%s.jpg", settings.file_root, settings.base_file_name);
        }
    }

    /* Only write files for specific formats */
    if (fb->vd->format_in == V4L2_PIX_FMT_YUYV ||
	    fb->vd->format_in == V4L2_PIX_FMT_Z16) {

    	/* Open and write the file */
    	FILE* p_file = fopen(temp_out_file_path, "w+");

    	if (p_file == NULL) {
        	panic("Can't write output image file.");
       	 	return;
    	}

    	// write the correct type of file
    	if (file_type == OUTPUT_FILE_TYPE_BMP) {
            size_t bytesPerPixel = (fb->vd->format_in == V4L2_PIX_FMT_Z16) ? 1 : 3;
            bmWriteBitmap(p_file, fb->vd->width, fb->vd->height, bytesPerPixel, data, data_len);
    	} else {
            fwrite(data, data_len, 1, p_file);
        }

    	fflush(p_file);
    	fclose(p_file);

    	/* Now that write is complete, rename the file */
    	rename(temp_out_file_path, out_file_path);
    }
}

bool parseDetectColor(const char *p_detect_color_string, uint32_t detect_color_len, detect_color_t* p_detect_color) {

    if (!p_detect_color_string || detect_color_len != DETECT_COLOR_LENGTH || p_detect_color_string[0] != '#') {
        return false;
    }

    // parse
    unsigned long hexVal = strtoul(&p_detect_color_string[1], NULL, 16);

    // extract rgb
    hexVal &= 0xFFFFFF;
    p_detect_color->red = hexVal >> 16;
    p_detect_color->green = (hexVal >> 8) & 0xFF;
    p_detect_color->blue = hexVal & 0xFF;

    printf("Converting %s -> r: %f g: %f b: %f\n", p_detect_color_string, p_detect_color->red, p_detect_color->green, p_detect_color->blue);

    return true;
}

void grab_frame(struct frame_buffer *fb, output_file_t file_type, detect_color_t detect_color, bool b_color_detect, float detect_tolerance) {
    uint8_t *buf = NULL;
    uint32_t buf_size = 0;

    if (file_type == OUTPUT_FILE_TYPE_BMP) {
        uint32_t buf_size = fb->vd->width * fb->vd->height;
        if (fb->vd->format_in == V4L2_PIX_FMT_YUYV) {
            buf_size *= 3;
        } else if (fb->vd->format_in == V4L2_PIX_FMT_Z16) {
            buf_size *= 1;
        }
        buf = malloc(buf_size);

    } else {
        buf = (uint8_t *) malloc(fb->vd->framebuffer_size);
        buf_size = fb->vd->framebuffer_size;
    }

    if (!buf) {
        perror("Couldn't allocate output frame data buffer");
        return;
    }

    size_t frame_size = 0;
    frame_size = capture_frame(fb->vd);

    if (frame_size > 0) {
        /* Process by input format type (output type is always JPEG) */
        switch (fb->vd->format_in) {
            case V4L2_PIX_FMT_YUYV:
                if (file_type == OUTPUT_FILE_TYPE_BMP) {
                    frame_size = compress_yuyv_to_bmp(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                      fb->vd->height);

                    // perform color detection, if enabled
                    if (b_color_detect) {
                        rgb_color_detection(buf, frame_size, fb->vd->width, fb->vd->height, &detect_color, detect_tolerance, true, true, true, settings.file_root);
                    }
                } else {
                    frame_size = compress_yuyv_to_jpeg(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                       fb->vd->height, fb->vd->jpeg_quality);
                }
                break;
            case V4L2_PIX_FMT_Z16:
                if (file_type == OUTPUT_FILE_TYPE_BMP) {
                    frame_size = compress_z16_to_bmp(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                     fb->vd->height, settings.mm_scale);
                } else {
                    frame_size = compress_z16_to_jpeg(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                      fb->vd->height, fb->vd->jpeg_quality, settings.mm_scale);
                }
                break;
            default:
                panic("Video device is using unknown format.");
                break;
        }

        write_frame(fb, buf, frame_size, file_type);
    }

    free(buf);
    buf = NULL;

    requeue_device_buffer(fb->vd);
}


int main(int argc, char *argv[]) {
    int i;
    struct frame_buffers *fbs;
    struct frame_buffer *fb;
    struct timespec ts;

    // output file type
    output_file_t file_type = OUTPUT_FILE_TYPE_NONE;

    double delta;
    static double fps_avg = 0.0f;
    double fps;

    detect_color_t detect_color;
    bool b_detect_color = false;

    bool calc_fps = false;

    bmInit();
    colorDetectInit();

    init_settings(argc, argv);

    // detect color
    b_detect_color = parseDetectColor(settings.detect_color, strlen(settings.detect_color), &detect_color);

    if (b_detect_color) {
        b_detect_color = calcNorms(&detect_color);
        if (!b_detect_color) {
            printf("%s: detect color disabled, invalid color specified", __func__);
        }
    }

    float color_detect_tolerance = ((float)settings.detect_tolerance) / 100.0f;

    // bound color-detect tolerance percentage
    if (color_detect_tolerance < 0.05f) {
        color_detect_tolerance = 0.05f;
    } else if (color_detect_tolerance > 0.90f) {
        color_detect_tolerance = 0.90f;
    }

    printf("%s: detect tolerance %f%% from %d\n", __func__, color_detect_tolerance, settings.detect_tolerance);

    // proflie fps
    if (settings.profile_fps != 0) {
        calc_fps = true;
    }

    // determine output file format
    if (strcmp(settings.file_format, "bmp") == 0) {
        file_type = OUTPUT_FILE_TYPE_BMP;
    } else {
        file_type = OUTPUT_FILE_TYPE_JPG;
    }

    if (settings.run_in_background) {
        daemonize();
    }

    init_signals();

    fbs = init_frame_buffers(settings.video_device_count, settings.video_device_file);

    while (is_running) {
        delta = gettime();
        for (i = 0; i < fbs->count; i++) {
            fb = &fbs->buffers[i];
            grab_frame(fb, file_type, detect_color, b_detect_color, color_detect_tolerance);
        }

        if (calc_fps) {
            fps = 1.0f / ((gettime() - delta) / fbs->count);
            if (fps_avg == 0.0f) {
                fps_avg = fps;
            } else {
                fps_avg += fps;
                fps_avg /= 2;
            }
            printf("%s: fps: %f\n", __func__, fps_avg);
        }

        delta = gettime() - delta;
        if (delta > 0) {
            double_to_timespec(delta, &ts);
            nanosleep(&ts, NULL);
        }
    }

    destroy_frame_buffers(fbs);

    cleanup_settings();

    return 0;
}

