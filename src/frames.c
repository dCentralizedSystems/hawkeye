
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "memory.h"
#include "frames.h"

void create_frame_buffer(struct frame_buffer *fb, size_t n) {
    int i;

    fb->buffer_size = n;
    fb->current_frame = -1;
    fb->frames = calloc(n, sizeof(struct frame));
    fb->vd = NULL;

    for (i = 0; i < fb->buffer_size; i++) {
        fb->frames[i].data = malloc(MIN_FRAME_SIZE);
        fb->frames[i].data_len = 0;
        fb->frames[i].data_buf_len = MIN_FRAME_SIZE;
    }
}

void destroy_frame_buffer(struct frame_buffer *fb) {
    int i;

    for (i = 0; i < fb->buffer_size; i++) {
        free(fb->frames[i].data);
    }

    free(fb->frames);
}


