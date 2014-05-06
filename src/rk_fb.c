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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "xf86.h"

#include "rk_fb.h"


int fetch_screen_info(rk_fb *rkfb) {
	return ioctl(rkfb->fb_fd, FBIOGET_VSCREENINFO, &rkfb->screen_info);
}

uint32_t rk_fb_get_screen_width(rk_fb *rkfb) {
	return rkfb->screen_info.xres;
}

uint32_t rk_fb_get_screen_height(rk_fb *rkfb) {
	return rkfb->screen_info.yres;
}

ssize_t rk_fb_get_offset_to_xvideo_mem(rk_fb *rkfb) {
	int w = rkfb->screen_info.xres_virtual * rkfb->screen_info.bits_per_pixel / 8;
	int h = rkfb->screen_info.yres_virtual;
	int offset = w * h *2;
	
	return offset;

}

rk_fb *rk_fb_init(ScreenPtr pScreen, const char *fb_device, void *xserver_fbmem) {
	rk_fb *rkfb;
	char *ovl_device;
	struct rk_fb_mem_inf mem_info;
	
	rkfb = calloc(sizeof(rk_fb), 1);
	if (!rkfb) {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB ERROR: Failed to allocate memory\n");
		return NULL;
	}
	
	if (!fb_device) {
		fb_device = "/dev/fb0";
	}
	
	rkfb->fb_fd = open(fb_device, O_RDWR);
	if (rkfb->fb_fd < 0) {
		xf86DrvMsg(pScreen->myNum, X_INFO,
		           "RK_FB ERROR: Failed to open the framebuffer device (%s)\n", fb_device);
		goto err;
	}
	
	if (ioctl(rkfb->fb_fd, FBIOGET_PHYMEMINFO, &mem_info)) {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB ERROR: Failed to get the framebuffer's phys. memory "
		                                   "information, probably not Rockchip hardware\n");
		goto err;
	}
	rkfb->fb_phy_addr = mem_info.yrgb;
	rkfb->fb_mem_len = mem_info.len;
	
	if (strncmp(fb_device, "/dev/fb0", strlen("/dev/fb0") + 1) == 0) {
		ovl_device = "/dev/fb1";
	} else if (strncmp(fb_device, "/dev/fb2", strlen("/dev/fb2") + 1) == 0) {
		ovl_device = "/dev/fb3";
	} else {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB ERROR: Unknown framebuffer device (%s)\n", fb_device);
		goto err;
	}
	
	if (fetch_screen_info(rkfb)) {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB ERROR: Failed to fetch screen info\n");
		goto err;
	}
	
	rkfb->ovl_fd = open(ovl_device, O_RDWR);
	if (rkfb->ovl_fd < 0) {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB WARNING: Failed to open the overlay device (%s)\n", ovl_device);
	}
	
	rkfb->rga_fd = open("/dev/rga", O_RDWR);
	if (rkfb->ovl_fd < 0) {
		xf86DrvMsg(pScreen->myNum, X_INFO, "RK_FB WARNING: Failed to open the RGA device\n");
	}
	
	rkfb->fb_mem = xserver_fbmem;
	
	rkfb->pScreen = pScreen;
	
	return rkfb;
	
err:
	free(rkfb);
	return NULL;
}

