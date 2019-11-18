cmake_minimum_required(VERSION 3.13)
project(hawkeye C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3 -Wall -Wl,-wrap,malloc,-wrap,realloc,-wrap,calloc,-wrap,strdup")

FIND_LIBRARY(jpeg REQUIRED)
FIND_LIBRARY(v4l2 REQUIRED)
FIND_LIBRARY(OpenSSL REQUIRED PATH /usr/lib/arm-linux-gnueabihf)
FIND_LIBRARY(apriltag REQUIRED PATH /usr/local/lib)

include_directories(src /usr/include /usr/include /usr/local/include ${JPEG_INCLUDE_DIR} ${V4L2_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${APRILTAG_INCLUDE_DIR})
link_directories(/usr/local/lib /usr/lib ${JPEG_LIBRARY_DIR} ${V4L2_LIBRARY_DIR} ${OPENSSL_LIBRARY_DIR} ${APRILTAG_LIBRARY_DIR})

add_executable(hawkeye
        src/config.c
        src/config.h
        src/daemon.c
        src/daemon.h
        src/frames.c
        src/frames.h
        src/http.c
        src/http.h
        src/huffman.h
        src/jpeg_utils.c
        src/jpeg_utils.h
        src/logger.c
        src/logger.h
        src/main.c
        src/main.h
        src/memory.c
        src/memory.h
        src/mime.h
        src/security.c
        src/security.h
        src/server.c
        src/server.h
        src/settings.c
        src/settings.h
        src/utils.c
        src/utils.h
        src/v4l2uvc.c
        src/v4l2uvc.h
        src/version.c
        src/version.h
	src/apriltag_process.c
	src/apriltag_process.h)

target_link_libraries(hawkeye jpeg v4l2 ssl crypto apriltag)

install(TARGETS hawkeye DESTINATION /usr/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

