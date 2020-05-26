#ifndef _BITMAP_H
#define _BITMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Initialize bitmap module
void bmInit(void);

// Creates and writes a .bmp image file to the specified file pointer.  Assumes pixel data is already in RGB LE format.
bool bmWriteBitmap(FILE *fp, uint32_t width, uint32_t height, uint8_t bytesPerPixel, void *p_buf, size_t imageSizeBytes);

#endif //_BITMAP_H
