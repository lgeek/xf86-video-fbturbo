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


Notes on video playback performance
-----------------------------------

For best performance, use mplayer compiled with NEON support. In some cases it seems to be more than two times faster compared to a version compiled without NEON support. To compile it on a Debian/Ubuntu-based OS, install the build dependencies with

    # apt-get build-dep mplayer

It might still be possible to build mplayer without installing the build dependencies, but features *will* be missing.

To configure mplayer with NEON and Thumb support, add the following configure options:

    --enable-thumb --enable-neon --extra-cflags="-mcpu=cortex-a9 -mfpu=neon"

Apart from the obvious factors like clock frequency and system load, there are a few others which heavily influence performance:

* memory bus width: I am one of the (hopefully) few owners of a RK3188 device with a 16 bit memory bus width [pipo x2](http://linux-rockchip.info/mw/index.php?title=PiPO_X2), as opposed to the more typical 32 bit bus width. This is essentially halving the memory bandwidth of the system, which has a very noticeable effect on system performance. The 16 bit system seems to have around 1000 MB/s memory bandwidth, while the 32 bit system has around 2000 MB/s memory bandwidth.

* framebuffer resolution, color depth and refresh rate: Since the framebuffer is stored in the main system memory, the CPU and DMA-enabled devices compete for memory access with the graphics subsystem, which pushes data from the framebuffer to the LCD port. 720p (16bpp) @ 60 Hz setting is going to use close to 105 MB/s of memory bandwidth, 1080p (16 bpp) @ 60 Hz is going to use around 237 MB/s and 1080p (32bpp) @ 60 Hz uses almost 475 MB/s. In the case of the system with a 16 bit memory bus width, this last setting halves the memory bandwidth available to the reset of the system, which seriously affects the performance of many workloads, including high resolution video playback.

* number of decoder threads: mplayer supports multithreaded decoding, but at least the version I'm using doesn't enable it by default. The following parameter needs to be passed:

    -lavdopts threads=N

where 4 is a good starting value for N on RK3188 (since it's quad-core).


Totally subjective experience with video playback
-------------------------------------------------

On a system with a 32 bit memory bus:

* Using a 1080p 32bpp framebuffer doesn't seem to have a noticeable effect on playback performance.
* All 720p videos I've tried play perfectly smoothly, with no noticeable late frames and without mplayer complaining that the system is too slow (with the NEON-enabled mplayer running 4 threads).
* The Ubuntu-provided armhf mplayer package (without NEON support) produces a reasonable viewing experience, but with some noticeable late frames.
* Very limited testing with 1080p (h264) files shows a reasonable viewing experience if frame dropping is enabled (and only with the NEON-enabled mplayer). It would be interesting to see if further refinement of mplayer code, mplayer playback options, memory settings or XV-related code can improve this.

On a system with a 16 bit memory bus:

* Using the 1080p 32bpp framebuffer pretty much breaks playback of any video larger than DVD resolution.
* 720p videos can be played well with the NEON-enabled mplayer, but with a few late frames if the framebuffer is configured as 1080p (16bpp) @ 60 Hz. With the framebuffer set to 1080p (16 bpp) @ 50 Hz, I haven't noticed any late frames.
* 1080p videos are most likely never going to work with software decoding on this platform.


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
