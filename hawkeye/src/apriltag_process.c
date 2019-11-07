#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apriltag_process.h"

char* apriltag_process(int format_in, unsigned char* buf, unsigned int buf_len) {
    char* p_results = (char*)malloc(64);

    memset(p_results, 0, 64);

    sprintf(p_results, "apriltag_process_results: %u\r\n", 0);

    return p_results;
}
