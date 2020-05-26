#include <stdlib.h>
#include <string.h>
#include "bitmap.h"

#pragma pack(push,1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBytes;
} bmFileHeader;

typedef struct {
    uint32_t biSize;
    uint32_t biWidth;
    uint32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    uint32_t biXPelsPerMeter;
    uint32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmInfoHeader;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} rgbColorTableEntry;
#pragma pack(pop)

// Default bitmap file header and bitmap info header
const static bmFileHeader bmFileHeaderTemplate = { 19778, 0, 0, 0, 54 };
const static bmInfoHeader bmInfoHeaderTemplate = { 40, 0, 0, 1, 24, 0, 0, 0, 0, 0, 0 };
static rgbColorTableEntry colorTable[256] = { 0 };

void bmInit(void) {
    // build color-table
    for (uint32_t i=0; i < 256; ++i) {
        colorTable[i].blue = i;
        colorTable[i].green = i;
        colorTable[i].red = i;
        colorTable[1].reserved = 0;
    }
}

bool bmWriteBitmap(FILE *fp, uint32_t width, uint32_t height, uint8_t bytesPerPixel, void *p_buf, size_t imageSizeBytes) {
    bmFileHeader hdr = bmFileHeaderTemplate;
    bmInfoHeader info = bmInfoHeaderTemplate;
    uint32_t stride = width * bytesPerPixel;
    uint32_t padding = 0;

    // update image parameters
    info.biBitCount = bytesPerPixel * 8;
    info.biWidth = width;
    info.biHeight = -height;

    if (!p_buf || !fp || imageSizeBytes == 0) {
        perror("Invalid params");
        printf("%s: imageSizeBytes == %lu\n", __func__, imageSizeBytes);
        return false;
    }

    // Check that passed image parameters make sense
    if (width * height * bytesPerPixel != imageSizeBytes) {
        perror( "image size incorrect");
        return false;
    }

    // Determine row padding and length
    if (stride % 4 != 0) {
        padding = 4 - (stride % 4);
        stride += padding;
    }

    // Allocate pixel buffer
    size_t output_bytes = height * stride;
    uint8_t *p_pix = (uint8_t*) malloc(output_bytes);

    if (!p_pix) {
        perror("Can't allocate memory for image file buffer");
        return false;
    }

    // Iterate over input data (width * height * bytesPerPixel in size)
//#define PER_PIXEL
#ifdef PER_PIXEL
    uint8_t *p_in = p_buf;
    uint8_t *p_out = p_pix;

    for (uint32_t h=0; h < height; ++h) {
        for (uint32_t w=0; w < width; w += bytesPerPixel) {
            memcpy(p_out, &p_in[h*w], bytesPerPixel);
            p_out += bytesPerPixel;

            if (w == width-1) {
                memset(p_out, 0, padding);
                p_out += padding;
            }
        }
    }
#else
    uint8_t *p_input = (uint8_t*)p_buf;
    size_t output_row_offset = 0;
    uint32_t input_width_bytes = width * bytesPerPixel;

    for (int h=0; h < height; ++h) {
        // copy a row of width pixels from the input image to the output image
        memcpy(&p_pix[output_row_offset], &p_input[h*input_width_bytes], input_width_bytes);

        if (padding) {
            memset(&p_pix[output_row_offset + width], 0, padding);
        }
        output_row_offset += stride;
    }
#endif

    // update bitmap info
    //info.biSizeImage = output_bytes;
    info.biSizeImage = 0;

    // color-table, if necessary
    uint32_t color_table_size = 0;

    if (bytesPerPixel == 1) {
        info.biClrUsed = 256;
        info.biClrImportant = 256;
        color_table_size = sizeof(colorTable);
    } else {
        info.biClrUsed = 0;
    }

    // update file size and offset
    hdr.bfOffBytes = sizeof(hdr) + sizeof(info) + color_table_size;
    hdr.bfSize = sizeof(hdr) + sizeof(info) + color_table_size + output_bytes;

    // Write the image bitmap file info
    if (!fwrite(&hdr, sizeof(hdr), 1, fp)) {
        return false;
    }

    // Write the image bitmap info
    if (!fwrite(&info, sizeof(info), 1, fp)) {
        return false;
    }

    // Write the color table, if necessary
    if (color_table_size != 0) {
        if (!fwrite((uint8_t*)&colorTable, color_table_size, 1, fp)) {
            return false;
        }
    }

    // Write the pixel data
    if (!fwrite(p_pix, output_bytes, 1, fp)) {
        return false;
    }

    // Free pixel data
    if (p_pix) {
        free(p_pix);
        p_pix = NULL;
    }

    return true;
}

