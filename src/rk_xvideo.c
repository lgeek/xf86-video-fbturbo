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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>

#include "xf86.h"
#include "xf86xv.h"
#include "fourcc.h"
#include <X11/extensions/Xv.h>

#include "fbdev_priv.h"
#include "rk_fb.h"
#include "rk_xvideo.h"

int rk_set_output_win(void *self, int drw_x, int drw_y, int drw_w, int drw_h) {
	rk_xvideo *par = (rk_xvideo *)self;

	struct fb_var_screeninfo fb1_conf;
	int ovl = 0;
	uint32_t addr[2];
	
	ioctl(par->rkfb->ovl_fd, FBIOGET_VSCREENINFO, &fb1_conf);
	fb1_conf.xoffset = par->src_x; // image (buffer) x offset
	fb1_conf.yoffset = par->src_y; // image (buffer) y offset
	fb1_conf.xres = par->src_w; // actual (buffer) x width
	fb1_conf.xres_virtual = par->src_stride;  // vir (stride)
	fb1_conf.yres = par->src_h; // actual (buffer) height
	fb1_conf.yres_virtual = par->src_h + par->src_y; // vir (stride) ?
	
	// DSP resolution, xsize << 8, ysize << 20, nonzero value overrides xres, yres
	fb1_conf.grayscale = (drw_h << 20) | (drw_w << 8);
	fb1_conf.activate = FB_ACTIVATE_FORCE;
	// DSP offset, format in the lower 8 bits, xpos << 8, ypos << 20
	fb1_conf.nonstd = HAL_PIXEL_FORMAT_YCrCb_NV12 | (drw_x << 8) | (drw_y << 20);
	if (ioctl(par->rkfb->ovl_fd, FBIOPUT_VSCREENINFO, &fb1_conf)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to send PUT_VSCREENINFO\n");
	}
	
	ovl = 1;
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_OVERLAY_STATE, &ovl)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to set RK_FBIOSET_OVERLAY_STATE\n");
		return -1;
	}
	
	ovl = 0;
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_CONFIG_DONE, &ovl)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to send SET_CONFIG_DONE\n");
		return -1;
	}
	
	ovl = 1;
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_ENABLE, &ovl)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "FBIOSET_ENABLE\n");
		return -1;
	}
	
	return 0;
}

int rk_set_input_par(void *self, int src_w, int src_h, int stride, int src_x, int src_y) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	par->src_x = src_x;
	par->src_y = src_y;
	par->src_w = src_w;
	par->src_h = src_h;
	par->src_stride = stride;
	
	return 0;
}

int rk_set_input_buf(void      *self,
                     uint32_t  y_off,
                     uint32_t  u_off,
                     uint32_t  v_off) {
	uint32_t addr[2];
                      
	rk_xvideo *par = (rk_xvideo *)self;
	
	addr[0] = par->rkfb->fb_phy_addr + y_off;
	addr[1] = par->rkfb->fb_phy_addr + min(v_off, u_off);
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_YUV_ADDR, addr)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to set win1 addr\n");
		return -1;
	}
	
	return 0;
}

int rk_hide_window(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;

	if (!par->enabled) {
		return 0;
	}
	
	int enabled = 0;
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_ENABLE, &enabled)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "FBIOSET_ENABLE\n");
	}
	par->enabled = FALSE;
}

int rk_show_window(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;

	if (par->enabled) {
		return 0;
	}

	int enabled = 1;
	if (ioctl(par->rkfb->ovl_fd, RK_FBIOSET_ENABLE, &enabled)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "FBIOSET_ENABLE\n");
	}
	par->enabled = TRUE;
}

int rk_set_colorkey(void *self, uint32_t colorkey) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	colorkey |= (1 << 24);
	if (ioctl(par->rkfb->fb_fd, FBIOPUT_SET_COLOR_KEY, &colorkey)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to set colorkey\n");
		return -1;
	}
	
	return 0;
}

int rk_disable_colorkey(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	uint32_t colorkey = 0;
	if (ioctl(par->rkfb->fb_fd, FBIOPUT_SET_COLOR_KEY, &colorkey)) {
		xf86DrvMsg(par->rkfb->pScreen->myNum, X_INFO, "Failed to set colorkey\n");
		return -1;
	}
	
	return 0;
}

extern void interleaved_copy_u8(void *dst, void *src1, void *src2, size_t len);

void rk_copy_buf(void *self, void *dst, const void *src, size_t n, int img_fmt) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	memcpy(dst, src, n*2/3);
	
	interleaved_copy_u8((uint8_t *)((uint32_t)dst + (uint32_t)n*2/3),
	                    (uint8_t *)((uint32_t)src + (uint32_t)n*(img_fmt == FOURCC_YV12 ? 5 : 4)/6),
	                    (uint8_t *)((uint32_t)src + (uint32_t)n*(img_fmt == FOURCC_YV12 ? 4 : 5)/6),
	                    n/3);
}

char *rk_get_fb_mem(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;

	return (char *)par->rkfb->fb_mem;
}

ssize_t rk_get_total_fb_size(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;

	return par->rkfb->fb_mem_len;
}

 
ssize_t rk_get_visible_fb_size(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;

	return rk_fb_get_offset_to_xvideo_mem(par->rkfb);
}

int rk_get_screen_width(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	return rk_fb_get_screen_width(par->rkfb);
}

int rk_get_screen_height(void *self) {
	rk_xvideo *par = (rk_xvideo *)self;
	
	return rk_fb_get_screen_height(par->rkfb);
}

rk_xvideo *rk_xvideo_init(rk_fb *rkfb) {
	int enable;
	
	if (!rkfb || (rkfb->fb_fd == -1) || (rkfb->ovl_fd == -1)) {
		return NULL;
	}
	
	rk_xvideo *self = calloc(sizeof(rk_xvideo), 1);
	
	self->intf.self = self;
	self->intf.set_yuv420_input_buffer = rk_set_input_buf;
	self->intf.set_input_par = rk_set_input_par;
	self->intf.set_output_window = rk_set_output_win;
	self->intf.show_window = rk_show_window;
	self->intf.hide_window = rk_hide_window;
	self->intf.set_colorkey = rk_set_colorkey;
	self->intf.disable_colorkey = rk_disable_colorkey;
	self->intf.copy_buffer = rk_copy_buf;
	self->intf.get_fb_mem = rk_get_fb_mem;
	self->intf.get_visible_fb_size = rk_get_visible_fb_size;
	self->intf.get_total_fb_size = rk_get_total_fb_size;
	self->intf.get_screen_width = rk_get_screen_width;
	self->intf.get_screen_height = rk_get_screen_height;
	self->intf.flags = XV_DOUBLEBUFFERING;
	self->rkfb = rkfb;
	
	enable = 0;
	if (ioctl(self->rkfb->ovl_fd, RK_FBIOSET_ENABLE, &enable)) {
		xf86DrvMsg(rkfb->pScreen->myNum, X_INFO, "RK_XVIDEO: Failed to set up the overlay\n");
		return NULL;
	}
	
	return self;
}

