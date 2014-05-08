/*
 * Copyright © 2014 Cosmin Gorgovan <cosmin [at] linux-geek [dot] org>
 * Based on fbturbo sunxi hw cursor driver:
 * Copyright © 2013 Siarhei Siamashka <siarhei.siamashka@gmail.com>
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

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86Cursor.h"
#include "cursorstr.h"

#include "fbdev_priv.h"

#include "uthash.h"
#include "rk_fb.h"
#include "rk_hwcursor.h"

static void RkShowCursor(ScrnInfoPtr pScrn)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    RkDispHardwareCursor * cursor = (RkDispHardwareCursor *)fPtr->RkDispHardwareCursor_private;
    int fb_fd = cursor->rkfb->fb_fd;
    
    int enabled = 1;
    if (ioctl(fb_fd, FBIOPUT_SET_CURSOR_EN, (char *)&enabled)) {
    	ErrorF("RkShowCursor: ioctl failed\n");
    }
}

static void RkHideCursor(ScrnInfoPtr pScrn)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    RkDispHardwareCursor * cursor = (RkDispHardwareCursor *)fPtr->RkDispHardwareCursor_private;
    int fb_fd = cursor->rkfb->fb_fd;
    
    int enabled = 0;
    if (ioctl(fb_fd, FBIOPUT_SET_CURSOR_EN, (char *)&enabled)) {
    	ErrorF("RkHideCursor: ioctl failed\n");
    }
}

static void RkSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    RkDispHardwareCursor * cursor = (RkDispHardwareCursor *)fPtr->RkDispHardwareCursor_private;
    int fb_fd = cursor->rkfb->fb_fd;
    
    struct fbcurpos pos;
    pos.x = (uint16_t)(x & 0xFFFF);
    if (y < 0) y = 0;
    pos.y = (uint16_t)(y & 0xFFFF);
    
    if (ioctl(fb_fd, FBIOPUT_SET_CURSOR_POS, (char *)&pos)) {
    	ErrorF("RkSetCursorPosition: ioctl failed\n");
    }
}

static void RkSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
	FBDevPtr fPtr = FBDEVPTR(pScrn);
    RkDispHardwareCursor * cursor = (RkDispHardwareCursor *)fPtr->RkDispHardwareCursor_private;
    int fb_fd = cursor->rkfb->fb_fd;
    struct fb_image img;

    img.bg_color = bg;
    img.fg_color = fg;
    
    ioctl(fb_fd, FBIOPUT_SET_CURSOR_CMAP, (char *)&img);
}

static void RkLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
	FBDevPtr fPtr = FBDEVPTR(pScrn);
    RkDispHardwareCursor * cursor = (RkDispHardwareCursor *)fPtr->RkDispHardwareCursor_private;
    int fb_fd = cursor->rkfb->fb_fd;
    unsigned char cp[256];
    
    for (int i = 0; i < 256; i++) {
    	cp[i] = bits[i];
   		for (int shift = 6; shift >= 0; shift -= 2) {
   			unsigned char val = (bits[i] >> shift) & 0x3;
   			switch(val) {
   				case 3:
   					val = 1;
   					break;
   				case 0:
   					val = 3;
   					break;
   			}
   			cp[i] &= ~(0x3 << shift);
   			cp[i] |= val << shift;
   		}
    }
    
    if (ioctl(fb_fd, FBIOPUT_SET_CURSOR_IMG, cp)) {
    	ErrorF("RkLoadCursorImg: ioctl failed\n");
    }
}

RkDispHardwareCursor *RkDispHardwareCursor_Init(rk_fb *rkfb)
{
    xf86CursorInfoPtr InfoPtr;
    RkDispHardwareCursor *private;
    int enabled;

    // probe for hardware support
    enabled = 0;
    if (!rkfb || ioctl(rkfb->fb_fd, FBIOPUT_SET_CURSOR_EN, (char *)&enabled)) {
    	ErrorF("RkDispHardwareCursor_Init: No hardware support\n");
    	return NULL;
    }

    if (!(InfoPtr = xf86CreateCursorInfoRec())) {
        ErrorF("RkDispHardwareCursor_Init: xf86CreateCursorInfoRec() failed\n");
        return NULL;
    }

    InfoPtr->ShowCursor = RkShowCursor;
    InfoPtr->HideCursor = RkHideCursor;
    InfoPtr->SetCursorPosition = RkSetCursorPosition;
    InfoPtr->SetCursorColors = RkSetCursorColors;
    InfoPtr->LoadCursorImage = RkLoadCursorImage;
    InfoPtr->MaxWidth = InfoPtr->MaxHeight = 32;
    InfoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1;

    if (!xf86InitCursor(rkfb->pScreen, InfoPtr)) {
        ErrorF("RkDispHardwareCursor_Init: xf86InitCursor(pScreen, InfoPtr) failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

    private = calloc(1, sizeof(RkDispHardwareCursor));
    if (!private) {
        ErrorF("RkDispHardwareCursor_Init: calloc failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

	private->rkfb = rkfb;
    private->hwcursor = InfoPtr;
    return private;
}

