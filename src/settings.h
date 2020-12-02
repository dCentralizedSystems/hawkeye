#ifndef __SETTINGS_H
#define __SETTINGS_H

// These should all be strings to be able to pass them through the option handler function
#define DEFAULT_FPS "6"
#define DEFAULT_WIDTH "640"
#define DEFAULT_HEIGHT "480"
#define DEFAULT_MM_SCALE "0"
#define DEFAULT_JPEG_QUALITY "80"
#define DEFAULT_VIDEO_DEVICE_FILE "/dev/video0"
#define DEFAULT_V4L2_FORMAT "z16"
#define DEFAULT_FILE_ROOT "."
#define DEFAULT_BASE_FILE_NAME "image"
#define DEFAULT_PROFILE_FPS "0"
#define DEFAULT_WRITE_DETECT_IMAGE "0"
#define DEFAULT_ENABLE_STRIPE_DETECT "0"

#define DETECT_COLOR_LENGTH (7)

struct settings {
	short run_in_background;
	int fps;
	int width;
	int height;
    int mm_scale;
	int jpeg_quality;
	char *file_root;
	char *base_file_name;
	int v4l2_format;
	int video_device_count;
	char *video_device_file;
	int profile_fps;

	// Stripe-detect parameters
	int enable_stripe_detect;
	int write_detect_image;
};

void init_settings(int argc, char *argv[]);
void cleanup_settings();
extern struct settings settings;

#endif
