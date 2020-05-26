
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "memory.h"
#include "logger.h"
#include "frames.h"
#include "v4l2uvc.h"
#include "image_utils.h"
#include "bitmap.h"
#include "server.h"
#include "utils.h"
#include "daemon.h"
#include "settings.h"

#define FRAME_BUFFER_LENGTH 8

#define WRITE_BMP

static int is_running = 1;

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

struct frame_buffers *init_frame_buffers(size_t device_count, char *device_names[]) {
    int i;
    struct frame_buffer *fb;
    struct frame_buffers *fbs;

    fbs = malloc(sizeof(struct frame_buffers));
    fbs->count = 0;
    fbs->buffers = calloc(device_count, sizeof(struct frame_buffer));

    for (i = 0; i < device_count; i++) {
        fb = &fbs->buffers[i];

        create_frame_buffer(fb, FRAME_BUFFER_LENGTH);
        if ((fb->vd = create_video_device(device_names[i], settings.width, settings.height, settings.fps, settings.v4l2_format, settings.jpeg_quality)) == NULL) {
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
#ifndef WRITE_BMP
        sprintf(temp_out_file_path, "%s/%s.jpg~", settings.file_root, settings.base_file_name);
        sprintf(out_file_path, "%s/%s.jpg", settings.file_root, settings.base_file_name);
#else
        sprintf(temp_out_file_path, "%s/%s.bmp~", settings.file_root, settings.base_file_name);
        sprintf(out_file_path, "%s/%s.bmp", settings.file_root, settings.base_file_name);
#endif
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

#ifndef WRITE_BMP
    	fwrite(data, data_len, 1, p_file);
#else
    	size_t bytesPerPixel = (fb->vd->format_in == V4L2_PIX_FMT_Z16) ? 1 : 3;
        bmWriteBitmap(p_file, fb->vd->width, fb->vd->height, bytesPerPixel, data, data_len);
#endif
    	fflush(p_file);
    	fclose(p_file);

    	/* Now that write is complete, rename the file */
    	rename(temp_out_file_path, out_file_path);
    }
}

void grab_frame(struct frame_buffer *fb) {

#ifndef WRITE_BMP
    unsigned char buf[fb->vd->framebuffer_size];
#else
    uint32_t buf_size = fb->vd->width * fb->vd->height;
    if (fb->vd->format_in == V4L2_PIX_FMT_YUYV) {
        buf_size *= 3;
    } else if (fb->vd->format_in == V4L2_PIX_FMT_Z16) {
        buf_size *= 1;
    }
    uint8_t *buf = malloc(buf_size);
    if (!buf) {
        perror("Couldn't allocate output frame data buffer");
        return;
    }
#endif

    size_t frame_size = 0;
    frame_size = capture_frame(fb->vd);

    if (frame_size <= 0) {
        log_it(LOG_ERROR, "Could not capture frame.");
    } else {
        /* Process by input format type (output type is always JPEG) */
        switch (fb->vd->format_in) {
            case V4L2_PIX_FMT_YUYV:
#ifndef WRITE_BMP
                frame_size = compress_yuyv_to_jpeg(buf, sizeof(buf), fb->vd->framebuffer, frame_size, fb->vd->width, fb->vd->height, fb->vd->jpeg_quality);
#else
                frame_size = compress_yuyv_to_bmp(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width, fb->vd->height);
#endif
                break;
            case V4L2_PIX_FMT_Z16:
#ifndef WRITE_BMP
                frame_size = compress_z16_to_jpeg(buf, sizeof(buf), fb->vd->framebuffer, frame_size, fb->vd->width, fb->vd->height, fb->vd->jpeg_quality, settings.mm_scale);
#else
                frame_size = compress_z16_to_bmp(buf, buf_size, fb->vd->framebuffer, frame_size, fb->vd->width, fb->vd->height, settings.mm_scale);
#endif
                break;
            default:
                panic("Video device is using unknown format.");
                break;
        }
    }

    write_frame(fb, buf, frame_size);

#ifdef WRITE_BMP
    free(buf);
    buf = NULL;
#endif

    requeue_device_buffer(fb->vd);

    //add_frame(fb, buf, frame_size);
}

int main(int argc, char *argv[]) {
    int i;
    struct frame_buffers *fbs;
    struct frame_buffer *fb;
    struct timespec ts;

    double delta;
    static double fps_avg = 0.0f;
    double fps;

    bmInit();

    init_settings(argc, argv);

    if (settings.run_in_background) {
        daemonize();
    }

    if (strlen(settings.pid_file) > 0 ) {
        write_pid(settings.pid_file, settings.user, settings.group);
    }   

    init_signals();

    open_log(settings.log_file, settings.log_level);

    fbs = init_frame_buffers(settings.video_device_count, settings.video_device_files);

    if (strlen(settings.log_file) > 0) {
        nchown(settings.log_file, settings.user, settings.group);
    }

    drop_privileges(settings.user, settings.group);

    while (is_running) {
        delta = gettime();
        for (i = 0; i < fbs->count; i++) {
            fb = &fbs->buffers[i];
            grab_frame(fb);
        }

        fps = 1.0f / ((gettime() - delta) / fbs->count);
        if (fps_avg == 0.0f) {
            fps_avg = fps;
        } else {
            fps_avg += fps;
            fps_avg /= 2;
        }
        printf("%s: fps: %f\n", __func__, fps_avg);

        delta = gettime() - delta;
        if (delta > 0) {
            double_to_timespec(delta, &ts);
            nanosleep(&ts, NULL);
        }
    }

    destroy_frame_buffers(fbs);

    log_it(LOG_INFO, "Shutting down.");

    if (strlen(settings.pid_file) > 0 ) {
        unlink(settings.pid_file);
    }

    cleanup_settings();

    close_log();

    return 0;
}

