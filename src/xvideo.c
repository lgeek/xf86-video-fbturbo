/*
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86xv.h"
#include "fourcc.h"
#include <X11/extensions/Xv.h>

#include "fbdev_priv.h"
#include "xvideo.h"
#include "sunxi_disp.h"

/*****************************************************************************/

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)  (sizeof((a)) / sizeof((a)[0]))
#endif

#define SIMD_ALIGN(s) (((s) + 15) & ~15)
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey;
xvideo_i *xvd = NULL;

/* Convert color key from 32bpp to the native format */
static uint32_t convert_color(ScrnInfoPtr pScrn, uint32_t color)
{
    uint32_t red = ((color >> 16) & 0xFF) >> (8 - pScrn->weight.red);
    uint32_t green = ((color >> 8) & 0xFF) >> (8 - pScrn->weight.green);
    uint32_t blue = (color & 0xFF) >> (8 - pScrn->weight.blue);
    return (red << pScrn->offset.red) | (green << pScrn->offset.green) |
           (blue << pScrn->offset.blue);
}

/* Clips to the visible screen; sets up both source and drawing coordinates */
Bool clip(short *src_x, short *src_y, short *drw_x, short *drw_y,
          short *src_w, short *src_h, short *drw_w, short *drw_h,
          short visible_w, short visible_h) {
          
    double v_ratio = (double)*src_h / (double)*drw_h; // source pixels per display pixels
    double h_ratio = (double)*src_w / (double)*drw_w;
    short x_src_offset, y_src_offset, x_margin, x_src_margin, y_margin, y_src_margin;
    
    // Is it completely off-screen?
    if (*drw_x < -*drw_w || *drw_y < -*drw_h || *drw_x > visible_w || *drw_y > visible_h) {
        return FALSE;
    }
    
    if (*drw_x < 0) {
        x_src_offset = (short)(h_ratio * -(*drw_x)) & (~1);
        *src_x += x_src_offset;
        *src_w -= x_src_offset;
        *drw_w += *drw_x;
        *drw_x = 0;
    }
    
    if (*drw_y < 0) {
        y_src_offset = v_ratio * -(*drw_y);
        *src_y += y_src_offset;
        *src_h -= y_src_offset;
        *drw_h += *drw_y;
        *drw_y = 0;
    }
    
    if ((*drw_x + *drw_w) > visible_w) {
        x_margin = (*drw_x + *drw_w) - visible_w;
        x_src_margin = h_ratio * x_margin;
        *drw_w -= x_margin;
        *src_w -= x_src_margin;
    }
    
    if ((*drw_y + *drw_h) > visible_h) {
        y_margin = (*drw_y + *drw_h) - visible_h;
        y_src_margin = h_ratio * y_margin;
        *drw_h -= y_margin;
        *src_h -= y_src_margin;
    }
    
    return TRUE;
}

/*****************************************************************************/

static void
xStopVideo(ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
    XVideo *self = XVIDEO(pScrn);

    if (xvd && cleanup) {
        xvd->hide_window(xvd->self);
        xvd->disable_colorkey(xvd->self);
        self->colorKeyEnabled = FALSE;
    }

    REGION_EMPTY(pScrn->pScreen, &self->clip);
}

static int
xSetPortAttributeOverlay(ScrnInfoPtr pScrn,
                         Atom        attribute,
                         INT32       value,
                         pointer     data)
{
    XVideo *self = XVIDEO(pScrn);

    if (attribute == xvColorKey && xvd) {
        self->colorKey = value;
        xvd->set_colorkey(xvd->self, self->colorKey);
        self->colorKeyEnabled = TRUE;
        REGION_EMPTY(pScrn->pScreen, &self->clip);
        return Success;
    }

    return BadMatch;
}


static int
xGetPortAttributeOverlay(ScrnInfoPtr pScrn,
                         Atom        attribute,
                         INT32      *value,
                         pointer     data)
{
    XVideo *self = XVIDEO(pScrn);

    if (attribute == xvColorKey) {
        *value = self->colorKey;
        return Success;
    }

    return BadMatch;
}

static void
xQueryBestSize(ScrnInfoPtr pScrn, Bool motion, short vid_w, short vid_h,
               short drw_w, short drw_h, unsigned int *p_w, unsigned int *p_h,
               pointer data)
{
    *p_w = drw_w;
    *p_h = drw_h;
}

static int
xPutImage(ScrnInfoPtr pScrn, short src_x, short src_y, short drw_x, short drw_y,
          short src_w, short src_h, short drw_w, short drw_h, int image,
          unsigned char *buf, short width, short height, Bool sync,
          RegionPtr clipBoxes, pointer data, DrawablePtr pDraw)
{
    XVideo *self = XVIDEO(pScrn);
    INT32 x1, x2, y1, y2;
    int y_offset, u_offset, v_offset;
    int y_stride, uv_stride, yuv_size;
    BoxRec dstBox;

    /* There used to be some clipping functionality here, but as far as I can tell the
       results were thrown away. Replaced below with a call to our own clipping function,
       which handles offsets and dimensions in both image buffer and drawing settings. */

    uv_stride = SIMD_ALIGN(width >> 1);
    y_stride  = uv_stride * 2;
    yuv_size  = y_stride * height + uv_stride * height;

    y_offset = 0;
    if (image == FOURCC_I420) {
        u_offset = y_stride * height;
        v_offset = (uv_stride * (height >> 1)) + u_offset;
    }
    else if (image == FOURCC_YV12) {
        v_offset = y_stride * height;
        u_offset = (uv_stride * (height >> 1)) + v_offset;
    }
    else {
        return BadImplementation;
    }

    if (xvd) {
        if (xvd->flags & XV_DOUBLEBUFFERING) {
            /* Try to fixup overlay offset */
            if (self->overlay_data_offs < xvd->get_visible_fb_size(xvd->self) ||
                self->overlay_data_offs + yuv_size > xvd->get_total_fb_size(xvd->self)) {
                self->overlay_data_offs = xvd->get_visible_fb_size(xvd->self);
            }
        }
        /* If it is still wrong (not enough offscreen memory), then fail */
        if (self->overlay_data_offs + yuv_size > xvd->get_total_fb_size(xvd->self))
            return BadImplementation;

        y_offset += self->overlay_data_offs;
        u_offset += self->overlay_data_offs;
        v_offset += self->overlay_data_offs;


        if (!clip(&src_x, &src_y, &drw_x, &drw_y, &src_w, &src_h, &drw_w, &drw_h,
                  xvd->get_screen_width(xvd->self), xvd->get_screen_height(xvd->self))) {
            return Success;
        }

        if (xvd->copy_buffer) {
            xvd->copy_buffer(xvd->self, xvd->get_fb_mem(xvd->self)
                             + self->overlay_data_offs, buf, yuv_size, image);
        } else {
            memcpy(xvd->get_fb_mem(xvd->self) + self->overlay_data_offs, buf, yuv_size);
        }

        /* Enable colorkey if it has not been already enabled */
        if (!self->colorKeyEnabled) {
            xvd->set_colorkey(xvd->self, self->colorKey);
            self->colorKeyEnabled = TRUE;
        }
        xvd->set_yuv420_input_buffer(xvd->self, y_offset, u_offset, v_offset);
        xvd->set_input_par(xvd->self, src_w, src_h, y_stride, src_x, src_y);
        xvd->set_output_window(xvd->self, drw_x, drw_y, drw_w, drw_h);
        xvd->show_window(xvd->self);

        if (xvd->flags & XV_DOUBLEBUFFERING) {
            /* Cycle through different overlay offsets (to prevent tearing) */
            self->overlay_data_offs += yuv_size;
        }
    }

    /* Update the areas filled with the color key */
    if (!REGION_EQUAL(pScrn->pScreen, &self->clip, clipBoxes)) {
        REGION_COPY(pScrn->pScreen, &self->clip, clipBoxes);
        xf86XVFillKeyHelperDrawable(pDraw, convert_color(pScrn, self->colorKey), clipBoxes);
    }
    
    return Success;
}

static int
xReputImage(ScrnInfoPtr pScrn, short src_x, short src_y, short drw_x, short drw_y,
          short src_w, short src_h, short drw_w, short drw_h,
          RegionPtr clipBoxes, pointer data, DrawablePtr pDraw)
{
    int stride = SIMD_ALIGN(src_w >> 1) * 2;
	
    if (!clip(&src_x, &src_y, &drw_x, &drw_y, &src_w, &src_h, &drw_w, &drw_h,
              xvd->get_screen_width(xvd->self), xvd->get_screen_height(xvd->self))) {
        return Success;
    }

    xvd->set_input_par(xvd->self, src_w, src_h, stride, src_x, src_y);
    xvd->set_output_window(xvd->self, drw_x, drw_y, drw_w, drw_h);

    return Success;
}

static int
xQueryImageAttributes(ScrnInfoPtr pScrn, int image,
                      unsigned short *w, unsigned short *h,
                      int *pitches, int *offsets)
{
    int height, width;
    int y_stride, uv_stride, yuv_size;

    width = *w = (*w + 1) & ~1;
    height = *h = (*h + 1) & ~1;

    uv_stride = SIMD_ALIGN(width >> 1);
    y_stride  = uv_stride * 2;
    yuv_size  = y_stride * height + uv_stride * height;

    if (pitches) {
        pitches[0] = y_stride;
        pitches[1] = uv_stride;
        pitches[2] = uv_stride;
    }

    if (offsets) {
        offsets[0] = 0;
        offsets[1] = y_stride * height;
        offsets[2] = (uv_stride * (height >> 1)) + offsets[1];
    }

    return yuv_size;
}

/*****************************************************************************/

static XF86VideoEncodingRec DummyEncoding[1] =
{
    { 0, "XV_IMAGE", XV_IMAGE_MAX_WIDTH, XV_IMAGE_MAX_HEIGHT, { 1, 1 } }
};

static XF86VideoFormatRec Formats[] = {
    {16, TrueColor}, {24, TrueColor}
};

static XF86ImageRec Images[] =
{
    XVIMAGE_YV12,
    XVIMAGE_I420
};

static XF86AttributeRec Attributes[] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
};

XVideo *XVideo_Init(ScreenPtr pScreen, xvideo_i *xv_i)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XVideo *self;
    XF86VideoAdaptorPtr adapt;

    /*if (!disp || !disp->layer_has_scaler) {
        xf86DrvMsg(pScreen->myNum, X_INFO,
                   "SunxiVideo_Init: no scalable layer available for XV\n");
        return NULL;
    }*/

    if (!(self = calloc(1, sizeof(XVideo)))) {
        xf86DrvMsg(pScreen->myNum, X_INFO, "XVideo_Init: calloc failed\n");
        return NULL;
    }

    if (!(self->adapt[0] = xf86XVAllocateVideoAdaptorRec(pScrn))) {
        free(self);
        return NULL;
    }

    adapt = self->adapt[0];

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "fbturbo Video Overlay";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding[0];
    adapt->nFormats = ARRAY_SIZE(Formats);
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion *) &self->port_privates[0];
    adapt->pAttributes = Attributes;
    adapt->nImages = ARRAY_SIZE(Images);
    adapt->nAttributes = ARRAY_SIZE(Attributes);

    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = xStopVideo;
    adapt->SetPortAttribute = xSetPortAttributeOverlay;
    adapt->GetPortAttribute = xGetPortAttributeOverlay;
    adapt->QueryBestSize = xQueryBestSize;
    adapt->PutImage = xPutImage;
    adapt->ReputImage = xReputImage;
    adapt->QueryImageAttributes = xQueryImageAttributes;

    xf86XVScreenInit(pScreen, &self->adapt[0], 1);

    xvColorKey = MAKE_ATOM("XV_COLORKEY");
    self->colorKey = 0x081018;
    REGION_NULL(pScreen, &self->clip);

    xvd = xv_i;
    self->overlay_data_offs = xvd->get_visible_fb_size(xvd->self);

    return self;
}

void XVideo_Close(ScreenPtr pScreen)
{
    if (xvd && xvd->close) {
        xvd->close(xvd->self);
    }
}
