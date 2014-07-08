XlessEGL
========

EGL on Linux without X11 using KernelModeSetting

This simple demo is taken from the chromium project. It was modified to run in VMware Workstation VMs, too.
No X11 / Wayland / GLUT / Xrandr dependencies.
Run from Virtual Terminal, this examples won't start under X11.

Compile with: 
   make

Example 1: eglkms
========

A bouncing red box.


Example 2: egltexkms
========

A bouncing textured box.

Example 3: eglbench
========

Benchmark for glDrawPixels, glTexSubImage2D, GL_ARB_pixel_buffer_object drawing to screen
