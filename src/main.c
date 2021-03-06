
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#include "memory.h"
#include "frames.h"
#include "v4l2uvc.h"
#include "image_utils.h"
#include "color_detect.h"
#include "bitmap.h"
#include "utils.h"
#include "daemon.h"
#include "settings.h"

#define FRAME_BUFFER_LENGTH     (8)
#define MAX_DETECT_COLORS       (2)

static int is_running = 1;

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

void write_frame(struct frame_buffer *fb, void *data, size_t data_len) {

    static char out_file_path[128] = {0};
    static char temp_out_file_path[128] = {0};

    if (out_file_path[0] == 0 && temp_out_file_path[0] == 0) {
        sprintf(temp_out_file_path, "%s/%s.jpg~", settings.file_root, settings.base_file_name);
        sprintf(out_file_path, "%s/%s.jpg", settings.file_root, settings.base_file_name);
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
    	fwrite(data, data_len, 1, p_file);

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

void grab_frame(struct frame_buffer *fb) {
    uint8_t *buf = NULL;
    uint32_t buf_size = 0;

    buf = (uint8_t *) malloc(fb->vd->framebuffer_size);
    buf_size = fb->vd->framebuffer_size;

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
                    frame_size = compress_yuyv_to_jpeg(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                       fb->vd->height, fb->vd->jpeg_quality,
                                                       (settings.enable_stripe_detect == 0) ? false : true,
                                                       (settings.write_detect_image == 0) ? false : true);
                break;
            case V4L2_PIX_FMT_Z16:
                frame_size = compress_z16_to_jpeg(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width,
                                                      fb->vd->height, fb->vd->jpeg_quality, settings.mm_scale);
                break;
            default:
                panic("Video device is using unknown format.");
                break;
        }

        write_frame(fb, buf, frame_size);
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

    double delta;
    static double fps_avg = 0.0f;
    double fps;

    bool calc_fps = false;

    bmInit();
    colorDetectInit();

    init_settings(argc, argv);

    // proflie fps
    if (settings.profile_fps != 0) {
        calc_fps = true;
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
            grab_frame(fb);
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

