#ifndef _BITMAP_H
#define _BITMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#pragma pack(push,1)
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} rgbColorTableEntry;
#pragma pack(pop)

// Initialize bitmap module
void bmInit(void);

// Creates and writes a .bmp image file to the specified file pointer.  Assumes pixel data is already in RGB LE format.
bool bmWriteBitmap(FILE *fp, uint32_t width, uint32_t height, uint8_t bytesPerPixel, void *p_buf, size_t imageSizeBytes);

// Writes a 256 color bitmap using the specified color table
bool bmWriteBitmapWithColorTable(FILE *fp, uint32_t width, uint32_t height, rgbColorTableEntry* p_color_table, uint32_t color_table_size, void *p_buf, size_t imageSizeBytes);

#endif //_BITMAP_H
