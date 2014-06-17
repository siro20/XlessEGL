XlessEGL
========

EGL on Linux without X11 using KernelModeSetting

This simple demo is taken from the chromium project. It was modified to run in VMware Workstation VMs, too.
No X11 / Wayland / GLUT / Xrandr dependencies.

Compile with: 
  gcc -o eglkms -I/usr/include/drm -lEGL -lGL -ldrm -lgbm eglkms.c

