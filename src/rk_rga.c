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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "xf86.h"

#include "interfaces.h"

#include "rk_fb.h"
#include "rk_rga.h"
#include "rga.h"

#define RGA_SIZE_THRESHOLD 10240 // (in + out bytes) only handle transfers larger than this

struct rga_req rga_req;

int rk_rga_blt(void *self, uint32_t *src_bits, uint32_t *dst_bits, int src_stride, int dst_stride,
              int src_bpp, int dst_bpp, int src_x, int src_y, int dst_x, int dst_y, int w, int h) {

	rk_rga *ctx = (rk_rga*)self;
	
	if (w <= 0 || h <= 0) {
		return 1;
	}
	
	/* Having different source and destination BPP would break some of the assumptions about
	   overlapped transfers. We only transfer within the framebuffer, so it's unlikely anyway */
	if (src_bpp != dst_bpp) {
		return 0;
	}
	
	/* RGA transfers have a somewhat large fixed overhead, so don't use them for small transfers. */
	if (w * h * (src_bpp + dst_bpp) < RGA_SIZE_THRESHOLD * 8) {
		return 0;
	}
	
	/* RGA doesn't have cache coherent access to memory, so we can't use it for operations outside
	   the framebuffer. We assume that framebuffer blits always have src_bits and dst_bits pointing
	   to the begging of the framebuffer mapping. This assumption seems to always be valid in
	   practice; examples showing otherwise are welcome. */
	/* It might be interesting to see if it's worth using RGA for very large transfers between main
	   memory and the framebuffer by flushing part of the cache. Or maybe we could try some tricks
	   for transfers from main memory to the framebuffer by using write-through caches. Watch out
	   for the ARM weakly ordered memory model. */
	/* If you disable this check, you get framebuffer corruption which shows the individual cache
	   lines which are out of sync from memory, it looks pretty cool */
	if (src_bits != ctx->rkfb->fb_mem || dst_bits != ctx->rkfb->fb_mem) {
		return 0;
	}
	
	/* It seems RGA can only copy starting from the top row, so if the source and destination areas
	   overlap and the source is above the destination, we have to either use a temporary buffer or
	   split the transfer in multiple chunks. Using a temporary buffer seems to be faster.
	   Any information about controlling the transfer unit size and direction would be most welcome. */
	if ((src_y <= dst_y) && ((src_y + h) > dst_y)
		&& ((src_x >= dst_x && src_x < (dst_x + w))
		|| ((src_x + w) > dst_x && (src_x + w) <= (dst_x + w)))
		&& !ctx->disable_overlapped_blts) {

		if (!rk_rga_blt(self, src_bits, dst_bits, src_stride, src_stride, src_bpp,
					  src_bpp, src_x, src_y, src_x, 1080, w, h)) {
			return 0;
		}
		if (!rk_rga_blt(self, src_bits, dst_bits, src_stride, dst_stride, src_bpp,
					  dst_bpp, src_x, 1080, dst_x, dst_y, w, h)) {
			return 0;
		}

		return 1;
	} // if (overlapped)
	
	switch(src_bpp) {
		case 16:
			rga_req.src.format = RK_FORMAT_RGB_565;
			rga_req.dst.format = RK_FORMAT_RGB_565;
			rga_req.src.vir_w = src_stride * 2;
			rga_req.dst.vir_w = dst_stride * 2;
			break;
		case 24:
			rga_req.src.format = RK_FORMAT_RGB_888;
			rga_req.dst.format = RK_FORMAT_RGB_888;
			rga_req.src.vir_w = src_stride * 4 / 3;
			rga_req.dst.vir_w = dst_stride * 4 / 3;
			break;
		case 32:
			rga_req.src.format = RK_FORMAT_RGBA_8888;
			rga_req.dst.format = RK_FORMAT_RGBA_8888;
			rga_req.src.vir_w = src_stride;
			rga_req.dst.vir_w = dst_stride;
			break;
		default:
			xf86DrvMsg(0, X_ERROR, "rkrga_blt: unsupported BPP\n");
			return 0;
	}

	if ((src_y + h) > 2048) {
		src_bits += src_y * src_stride;
		src_y = 0;
	}
	rga_req.src.yrgb_addr = (uint32_t)src_bits;
    rga_req.src.act_w = w;
    rga_req.src.act_h = h;
    rga_req.src.x_offset = src_x;
    rga_req.src.y_offset = src_y;
    rga_req.src.vir_h = src_y + h;

    if ((dst_y + h) > 2048) {
		dst_bits += dst_y * dst_stride;
		dst_y = 0;
	}
    rga_req.dst.yrgb_addr = (uint32_t)dst_bits;
    rga_req.dst.act_w = w;
    rga_req.dst.act_h = h;
    rga_req.dst.x_offset = dst_x;
    rga_req.dst.y_offset = dst_y;
    rga_req.dst.vir_h = dst_y + h;
    
	if (ioctl(ctx->rkfb->rga_fd, RGA_BLIT_SYNC, (char *)&rga_req) != 0) {
		xf86DrvMsg(0, X_INFO, "ioctl failed\n");
		return 0;
	}
	return 1;
}

/* Because rga_req is a large struct (212 bytes) and at the same time we only change a few
   of the fields for each separate request, we can remove some overhead by using a global
   structure prefilled with the fixed value fields (most of which are 0). */
void prepare_rga_req_struct() {
	rga_req.render_mode = bitblt_mode;
	rga_req.rotate_mode = rotate_mode0; // NO_ROTATE
	rga_req.mmu_info.mmu_en = 1;
	rga_req.mmu_info.mmu_flag = 0x21;
	rga_req.clip.xmin = 0;
    rga_req.clip.xmax = RGA_XMAX;
    rga_req.clip.ymin = 0;
    rga_req.clip.ymax = RGA_YMAX;
    rga_req.PD_mode = 1;
    rga_req.src_trans_mode = 0;

    rga_req.sina = 0;
    rga_req.cosa = 65536;
    rga_req.alpha_rop_flag = 0;
    
    rga_req.src.endian_mode = 1;
    rga_req.dst.endian_mode = 1;
}

rk_rga *rk_rga_init(rk_fb *rkfb)
{
	if (rkfb->rga_fd < 0) {
		xf86DrvMsg(rkfb->pScreen->myNum, X_INFO, "RGA device not found\n");
		return NULL;
	}

	rk_rga *ctx = calloc(sizeof(rk_rga), 1);

	prepare_rga_req_struct();
    
    if (rk_fb_get_offset_to_xvideo_mem(rkfb) > rkfb->fb_mem_len) {
    	xf86DrvMsg(rkfb->pScreen->myNum, X_INFO, "Disabling acceleration for overlapped "
		                                         "blits due to unsufficient framebuffer space\n");
    	ctx->disable_overlapped_blts = TRUE;
    }
    ctx->rkfb = rkfb;
    ctx->blt2d.self = ctx;
    ctx->blt2d.overlapped_blt = rk_rga_blt;
    
    return ctx;
}

