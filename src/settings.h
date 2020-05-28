#ifndef __SETTINGS_H
#define __SETTINGS_H

// These should all be strings to be able to pass them through the option handler function
#define DEFAULT_FPS "6"
#define DEFAULT_WIDTH "640"
#define DEFAULT_HEIGHT "480"
#define DEFAULT_MM_SCALE "0"
#define DEFAULT_JPEG_QUALITY "80"
#define DEFAULT_VIDEO_DEVICE_FILES "/dev/video0"
#define DEFAULT_V4L2_FORMAT "z16"
#define DEFAULT_FILE_FORMAT "jpg"
#define DEFAULT_FILE_ROOT "."
#define DEFAULT_BASE_FILE_NAME "image"
#define DEFAULT_PROFILE_FPS "0"
#define DEFAULT_DETECT_COLOR "#5A6429"
#define DEFAULT_DETECT_TOLERANCE "35"

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
	char *file_format;
	int video_device_count;
	char **video_device_files;
	int profile_fps;

	// Color-detect parameters
	char *detect_color;
	int detect_tolerance;
};

void init_settings(int argc, char *argv[]);
void cleanup_settings();
extern struct settings settings;

#endif