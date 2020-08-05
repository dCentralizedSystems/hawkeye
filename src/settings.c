#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <linux/videodev2.h>

#include "memory.h"
#include "version.h"
#include "config.h"
#include "utils.h"
#include "logger.h"

#include "settings.h"

struct settings settings;

static void normalize_path(char **path, const char *error) {
    char tmp_path[PATH_MAX];

    if (!strlen(*path)) {
        return;
    }

    if (NULL == realpath(*path, tmp_path)) {
        log_syslog_panic(error);
    }

    free(*path);
    *path = strdup(tmp_path);
}

void print_usage() {
    fprintf(stdout, "Usage: %s [-d] [-P pidfile]\n", program_name);
    fprintf(stdout, "       [-u user] [-g group] [-F fps] [-D video-device] [-W width]\n");
    fprintf(stdout, "       [-H height] [-j jpeg-quality] [-L log-level] [-f format] [-A user:pass]\n");
    fprintf(stdout, "       [-r file-root] [-b base_file_name] [-m mm-scale] [-P profile-fps] [-B #RRGGBB]  [-B #RRGGBB] [-M (0-10000)]\n");
    fprintf(stdout, "       [-T detect-tolerance-percent]\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage: %s [--daemon]\n", program_name);
    fprintf(stdout, "       [--pid=path][--user=user] [--group=group]\n");
    fprintf(stdout, "       [--fps=fps][--device=video-device] [--width=width] [--height=height]\n");
    fprintf(stdout, "       [--quality=quality] [--log-level=log-level] [--format=format]\n");
    fprintf(stdout, "       [--file-root=file-root] [--base-file-name=base-file-name]\n");
    fprintf(stdout, "       [--mm-scale=mm_scale] [--profile-fps=profile-fps] [--detect-color1=#RRGGBB] [--detect-color2=#RRGGBB] \n");
    fprintf(stdout, "       [--detect-tolerance=detect-tolerance-percent] [--min-detect-conf=[min blob pixels in 10000ths of total image pixels]\n");

    fprintf(stdout, "Usage: %s [-h]\n", program_name);
    fprintf(stdout, "Usage: %s [-v]\n", program_name);
    fprintf(stdout, "\n");
    fprintf(stdout, "devices is a : separated list of video devices, such as\n");
    fprintf(stdout, "for example \"/dev/video0:/dev/video1\".\n");
    fprintf(stdout, "log-level can be debug, info, warning, or error.\n");
    fprintf(stdout, "format can be yuv or z16.  Output file is jpg\n");
}

void init_settings(int argc, char *argv[]) {
    struct config *conf;
    char *v4l2_format;
    short display_version, display_usage;

    conf = create_config();

    add_config_item(conf, 'd', "daemon", CONFIG_BOOL, &settings.run_in_background, "0");
    add_config_item(conf, 'F', "fps", CONFIG_INT, &settings.fps, DEFAULT_FPS);
    add_config_item(conf, 'P', "profile-fps", CONFIG_INT, &settings.profile_fps, DEFAULT_PROFILE_FPS);
    add_config_item(conf, 'B', "detect-color1", CONFIG_STR, &settings.detect_color1, DEFAULT_DETECT_COLOR1);
    add_config_item(conf, 'C', "detect-color2", CONFIG_STR, &settings.detect_color2, DEFAULT_DETECT_COLOR2);
    add_config_item(conf, 'T', "detect-tolerance", CONFIG_INT, &settings.detect_tolerance, DEFAULT_DETECT_TOLERANCE);
    add_config_item(conf, 'M', "min-detect-conf", CONFIG_INT, &settings.min_detect_conf, DEFAULT_MIN_DETECT_CONF);
    add_config_item(conf, 'Q', "write-detect-image", CONFIG_BOOL, &settings.write_detect_image, "0");
    add_config_item(conf, 'W', "width", CONFIG_INT, &settings.width, DEFAULT_WIDTH);
    add_config_item(conf, 'H', "height", CONFIG_INT, &settings.height, DEFAULT_HEIGHT);
    add_config_item(conf, 'm', "mm-scale", CONFIG_INT, &settings.mm_scale, DEFAULT_MM_SCALE);
    add_config_item(conf, 'j', "quality", CONFIG_INT, &settings.jpeg_quality, DEFAULT_JPEG_QUALITY);
    add_config_item(conf, 'r', "file-root", CONFIG_STR, &settings.file_root, DEFAULT_FILE_ROOT);
    add_config_item(conf, 'b', "base-file-name", CONFIG_STR, &settings.base_file_name, DEFAULT_BASE_FILE_NAME);
    add_config_item(conf, 'f', "format", CONFIG_STR, &v4l2_format, DEFAULT_V4L2_FORMAT);
    add_config_item(conf, 'D', "device", CONFIG_STR, &settings.video_device_file, DEFAULT_VIDEO_DEVICE_FILE);
    add_config_item(conf, 'h', "help", CONFIG_BOOL, &display_usage, "0");
    add_config_item(conf, 'v', "version", CONFIG_BOOL, &display_version, "0");

    // Read command line options to get the location of the config file
    read_command_line(conf, argc, argv);

    // Read command line options again to overwrite config file values
    read_command_line(conf, argc, argv);

    destroy_config(conf);

    // Set format
    settings.v4l2_format = V4L2_PIX_FMT_YUYV;
    if (strcmp(v4l2_format, "yuv") == 0) {
        settings.v4l2_format = V4L2_PIX_FMT_YUYV;
    }
    if (strcmp(v4l2_format, "z16") == 0) {
        settings.v4l2_format = V4L2_PIX_FMT_Z16;
    }

    // Parse detect colors
    if (settings.detect_color1 == NULL || strlen(settings.detect_color1) != DETECT_COLOR_LENGTH) {
        settings.detect_color1 = DEFAULT_DETECT_COLOR1;
        settings.detect_color_count = 1;
    }
    if (settings.detect_color2 != NULL) {
        if (strlen(settings.detect_color2) == DETECT_COLOR_LENGTH) {
            settings.detect_color_count = 2;
        }
    }

    // Parse min detect confidence
    if (settings.min_detect_conf > 10000) {
        settings.min_detect_conf = 10000;
    }

    if (settings.min_detect_conf <=0) {
        settings.min_detect_conf = 4;
    }

    // Parse video devices
    settings.video_device_count = 1;

    free(v4l2_format);

    settings.jpeg_quality = max(1, min(100, settings.jpeg_quality));
    settings.fps = max(1, min(50, settings.fps));

    normalize_path(&settings.file_root, "The file-root you specified does not exist");

    if (display_usage) {
        print_usage();
        exit(0);
    }
}

void cleanup_settings() {
}

