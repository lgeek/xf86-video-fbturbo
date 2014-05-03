/*
 * Copyright © 2013 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 * Copyright © 2014 Cosmin Gorgovan <cosmin [at] linux-geek [dot] org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INTERFACES_H
#define INTERFACES_H

/* A simple interface for 2D graphics operations */
typedef struct {
    void *self; /* The pointer which needs to be passed to functions */
    /*
     * A counterpart for "pixman_blt", which supports overlapped copies.
     * Except for the new "self" pointer, the rest of arguments are
     * exactly the same.
     */
    int (*overlapped_blt)(void     *self,
                          uint32_t *src_bits,
                          uint32_t *dst_bits,
                          int       src_stride,
                          int       dst_stride,
                          int       src_bpp,
                          int       dst_bpp,
                          int       src_x,
                          int       src_y,
                          int       dst_x,
                          int       dst_y,
                          int       w,
                          int       h);
} blt2d_i;

/* An interface for XVideo */
#define XV_DOUBLEBUFFERING (1 << 0)

typedef struct {
    void *self; /* This pointer gets passed back to the functions */
    uint32_t flags;
    
    int (*set_yuv420_input_buffer)(void     *self,
                                   uint32_t  y_offset_in_framebuffer,
                                   uint32_t  u_offset_in_framebuffer,
                                   uint32_t  v_offset_in_framebuffer);
    int (*set_input_par)(void *self, int w, int h, int stride, int x, int y);
    int (*set_output_window)(void *self, int x, int y, int w, int h);

    int (*show_window)(void *self);
    int (*hide_window)(void *self);
    
    int (*set_colorkey)(void *self, uint32_t color);
    int (*disable_colorkey)(void *self);
    
    /* optional, if NULL, memcpy is used to copy straight from the output
       buffer of the application */
    void (*copy_buffer)(void *self, void *dest, const void *src, size_t n);
    
    int (*get_screen_width)(void *self);
    int (*get_screen_height)(void *self);
    char *(*get_fb_mem)(void *self);
    ssize_t (*get_visible_fb_size)(void *self);
    ssize_t (*get_total_fb_size)(void *self);
    
    void (*close)(void *self);
} xvideo_i;

#endif
