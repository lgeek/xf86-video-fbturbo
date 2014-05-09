RK3188 support for xf86-video-fbturbo
=====================================

Prerequisites
-------------

* You should be a developer or at least comfortable building stuff from source and debugging it. Not for the faint-hearted.
* A patched kernel. You can get it [here](https://github.com/lgeek/Linux3188). Alternatively, if you'd like to cherry pick the patches into another kernel tree, each relevant one is described in the README file of that repository. My initial intention was not to require any kernel patches, but various things were unsupported or broken without feasible work-arounds. Warning, trying to run this driver on a kernel without the required patches might put the system in a state that requires a power cycle to recover. There should be no risk of hardware damage, but all the usual disclaimers apply: use at your own risk.

Installation
------------

Boot the patched kernel and then follow the [standard fbturbo instructions](https://github.com/ssvb/xf86-video-fbturbo/wiki/Installation).

What works
----------

* hardware accelerated [XV](https://en.wikipedia.org/wiki/X_video_extension) driver, with which does image scaling and colorspace conversion in hardware (using the LCDC), now with multi-buffering support
* [RGA](http://linux-rockchip.info/mw/index.php?title=3d2dAcceleration#RGA_support)-accelerated framebuffer-to-framebuffer blits
* hardware cursor support (using the LCDC)


What does that mean?!
---------------------

* You can watch videos scaled up or down, including fullscreen. 720p videos scaled up to fullscreen on a 1080p display work quite nicely. I would strongly recommend to use mplayer compiled with NEON support.
* Scrolling, especially of large surfaces, is faster. Note that some benchmarks (e.g. x11perf -scroll500) will show slightly poorer results compared to a software implementation. However, since the CPU is idle during blits, it can service other processes. Under real world usage, the GUI should feel more responsive.
* You can see the cursor on top of the video overlay (as opposed to a software cursor which would be hidden). It can also be displayed on top of windows with rapidly changing contents.

Yet to be tested
----------------

* I haven't actually used the published instructions and code (kernel and fbturbo) to set up a clean system. There's a small chance I forgot to include some patch. Let me know whether it worked for you. 
* ~~Use in any other color depth configuration apart from 16 bit (RGB565).~~ Update: 32 BPP appears to be working correctly; 24 BPP appears to be working correctly, but the pixman version used by my Ubuntu installation doesn't quite like this unusual configuration
* Mali accelerated 3D graphics. I have no use for it, so I don't have the Mali drivers set up.

Known issues
------------

* Minor visual glitches when actively moving XV windows which are partially offscreen. Everything should recover when a window is left still. This seems to be a regression introduced when I was doing some refactoring, I'll get it fixed somehow.
* The video layer is always above other content (i.e. the contents of the mplayer window behave as if it would be set as 'always on top'). There is some inactive code for implementing proper behavior using color keying, but it has worse issues. :)

Thanks
------

[ssvb](https://github.com/ssvb)
