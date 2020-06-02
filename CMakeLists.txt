cmake_minimum_required(VERSION 3.13)
project(hawkeye C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O0 -Wall -Wl,-wrap,malloc,-wrap,realloc,-wrap,calloc,-wrap,strdup")

FIND_LIBRARY(jpeg REQUIRED)
FIND_LIBRARY(v4l2 REQUIRED)

include_directories(src /usr/include /usr/include /usr/local/include ${JPEG_INCLUDE_DIR} ${V4L2_INCLUDE_DIR})
link_directories(/usr/local/lib /usr/lib ${JPEG_LIBRARY_DIR} ${V4L2_LIBRARY_DIR})

add_executable(hawkeye
        src/config.c
        src/config.h
        src/daemon.c
        src/daemon.h
        src/frames.c
        src/frames.h
        src/image_utils.c
        src/image_utils.h
        src/main.c
        src/main.h
        src/memory.c
        src/memory.h
        src/settings.c
        src/settings.h
        src/utils.c
        src/utils.h
        src/v4l2uvc.c
        src/v4l2uvc.h
        src/version.c
        src/version.h
        src/bitmap.h
        src/bitmap.c src/color_detect.h src/color_detect.c)

target_link_libraries(hawkeye jpeg v4l2 m)

install(TARGETS hawkeye DESTINATION /usr/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

