/*
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


#ifndef RK_FB_H
#define RK_FB_H

#include <linux/fb.h>

#define RK_FBIOSET_YUV_ADDR         0x5002
#define RK_FBIOGET_PANEL_SIZE       0x5001
#define RK_FBIOSET_OVERLAY_STATE    0x5018
#define RK_FBIOSET_ENABLE           0x5019
#define RK_FBIOGET_ENABLE           0x5020
#define FBIOPUT_FBPHYADD            0x4608
#define FBIOPUT_SET_CURSOR_EN       0x4609
#define FBIOPUT_SET_CURSOR_IMG      0x460a
#define FBIOPUT_SET_CURSOR_POS      0x460b
#define FBIOPUT_SET_CURSOR_CMAP     0x460c
#define RK_FBIOGET_OVERLAY_STATE    0X4619
#define FBIOGET_PHYMEMINFO          0x461d
#define FBIOPUT_SET_COLOR_KEY       0x461f
#define RK_FBIOSET_CONFIG_DONE      0x4628

enum {
    HAL_PIXEL_FORMAT_YCrCb_NV12         = 0x20, // YUY2
    HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO   = 0x21, // YUY2
};

typedef struct {
	uint32_t   fb_phy_addr;
	uint32_t   *fb_mem;
	ssize_t    fb_mem_len;

	int        fb_fd;
	int        ovl_fd;
	int        rga_fd;
	
	ScreenPtr  pScreen;
	
	struct fb_var_screeninfo screen_info;
} rk_fb;

struct rk_fb_mem_inf {
	uint32_t yrgb;
	uint32_t cbr;
	uint32_t len;
};

rk_fb *rk_fb_init(ScreenPtr pScreen, const char *fb_device, void *xserver_fbmem);
uint32_t rk_fb_get_screen_width(rk_fb *rkfb);
uint32_t rk_fb_get_screen_height(rk_fb *rkfb);
ssize_t rk_fb_get_offset_to_xvideo_mem(rk_fb *rkfb);

#endif
