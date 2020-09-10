//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#include "fb2png.h"
#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int fb2png_exec(char *program, char *fbdevice, char *pngname)
{
    //--------------------------------------------------------------------
    int fbfd = open(fbdevice, O_RDWR);

    if (fbfd == -1)
    {
        fprintf(stderr,
                "%s: cannot open framebuffer - %s",
                program,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct fb_fix_screeninfo finfo;

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        fprintf(stderr,
                "%s: reading framebuffer fixed information - %s",
                program,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct fb_var_screeninfo vinfo;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        fprintf(stderr,
                "%s: reading framebuffer variable information - %s",
                program,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if ((vinfo.bits_per_pixel != 16) &&
        (vinfo.bits_per_pixel != 24) &&
        (vinfo.bits_per_pixel != 32))
    {
        fprintf(stderr, "%s: only 16, 24 and 32 ", program);
        fprintf(stderr, "bits per pixels supported\n");
        exit(EXIT_FAILURE);
    }

    uint8_t *memp = static_cast<uint8_t *>(mmap(0,
                      finfo.smem_len,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fbfd,
                      0));

    close(fbfd);
    fbfd = -1;

    if (memp == MAP_FAILED)
    {
        fprintf(stderr,
                "%s: failed to map framebuffer device to memory - %s",
                program,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    uint8_t *fbp = static_cast<uint8_t *>(memp);
    memp = NULL;

    //--------------------------------------------------------------------

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL,
                                                  NULL,
                                                  NULL);

    if (png_ptr == NULL)
    {
        fprintf(stderr,
                "%s: could not allocate PNG write struct\n",
                program);
        exit(EXIT_FAILURE);
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL)
    {
        fprintf(stderr,
                "%s: could not allocate PNG info struct\n",
                program);
        exit(EXIT_FAILURE);
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);

        fprintf(stderr, "%s: error creating PNG\n", program);
        exit(EXIT_FAILURE);
    }

    FILE *pngfp = fopen(pngname, "wb");

    if (pngfp == NULL)
    {
        fprintf(stderr, "%s: Unable to create %s\n", program, pngname);
        exit(EXIT_FAILURE);
    }

    png_init_io(png_ptr, pngfp);

    png_set_IHDR(
        png_ptr,
        info_ptr,
        vinfo.xres,
        vinfo.yres,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    png_bytep png_buffer = static_cast<png_bytep>(malloc(vinfo.xres * 3 * sizeof(png_byte)));

    if (png_buffer == NULL)
    {
        fprintf(stderr, "%s: Unable to allocate buffer\n", program);
        exit(EXIT_FAILURE);
    }

    //--------------------------------------------------------------------

    int r_mask = (1 << vinfo.red.length) - 1;
    int g_mask = (1 << vinfo.green.length) - 1;
    int b_mask = (1 << vinfo.blue.length) - 1;

    int bytes_per_pixel = vinfo.bits_per_pixel / 8;

    unsigned int y = 0;

    for (y = 0; y < vinfo.yres; y++)
    {
        unsigned int x;

        for (x = 0; x < vinfo.xres; x++)
        {
            int pb_offset = 3 * x;

            size_t fb_offset = x * (bytes_per_pixel)
                             + y * finfo.line_length;

            uint32_t pixel = 0;

            switch (vinfo.bits_per_pixel)
            {
            case 16:

                pixel = *((uint16_t *)(fbp + fb_offset));
                break;

            case 24:

                pixel += *(fbp + fb_offset);
                pixel += *(fbp + fb_offset + 1) << 8;
                pixel += *(fbp + fb_offset + 2) << 16;
                break;

            case 32:

                pixel = *((uint32_t *)(fbp + fb_offset));
                break;

            default:

                // nothing to do
                break;
            }

            png_byte r = (pixel >> vinfo.red.offset) & r_mask;
            png_byte g = (pixel >> vinfo.green.offset) & g_mask;
            png_byte b = (pixel >> vinfo.blue.offset) & b_mask;

            png_buffer[pb_offset] = (r * 0xFF) / r_mask;
            png_buffer[pb_offset + 1] = (g * 0xFF)  / g_mask;
            png_buffer[pb_offset + 2] = (b * 0xFF)  / b_mask;
        }

        png_write_row(png_ptr, png_buffer);
    }

    //--------------------------------------------------------------------

    free(png_buffer);
    png_buffer = NULL;
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(pngfp);

    //--------------------------------------------------------------------

    munmap(fbp, finfo.smem_len);

    //--------------------------------------------------------------------

    return 0;
}


int fb2png_defaults(){
    return fb2png_exec((char*)"fb2png", (char*)"/dev/fb0", (char*)"/tmp/fb.png");
}

int fb2png(int argc, char *argv[]){
    char *program = argv[0];
    char *fbdevice = (char*)"/dev/fb0";
    char *pngname = (char*)"/tmp/fb.png";

    int opt = 0;

    //--------------------------------------------------------------------

    while ((opt = getopt(argc, argv, "d:p:")) != -1)
    {
        switch (opt)
        {
        case 'd':

            fbdevice = optarg;
            break;

        case 'p':

            pngname = optarg;
            break;

        default:

            fprintf(stderr,
                    "Usage: %s [-d device] [-p pngname]\n",
                    program);
            exit(EXIT_FAILURE);
            break;
        }
    }
    return fb2png_exec(program, fbdevice, pngname);
}
