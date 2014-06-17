/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2014 Patrick Rudolph
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <gbm.h>
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

struct kms {
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeModeInfo mode;
	uint32_t crtc_id;
};

GLfloat x = 1.0;
GLfloat y = 1.0;
GLfloat xstep = 1.0f;
GLfloat ystep = 1.0f;
GLfloat rsize = 50;

int quit = 0;

static EGLBoolean
setup_kms(int fd, struct kms *kms)
{
	drmModeRes *resources;
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;

	int i,j;

	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return EGL_FALSE;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (connector == NULL)
		continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		connector->count_modes > 0)
		{
			printf("using resources->connectors[%d]\n",i);
			printf("resources->connectors[%d]->count_modes %d\n",i,connector->count_modes);
			printf("resources->connectors[%d]->encoder_id %d\n",i,connector->encoder_id);
			printf("resources->connectors[%d]->count_encoders %d\n",i,connector->count_encoders);
			break;
		}
		drmModeFreeConnector(connector);
	}
   
	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found.\n");
		return EGL_FALSE;
	}
	
	for (i = 0; i < connector->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, connector->encoders[i]);

		if (encoder == NULL)
			continue;

		if ((encoder->encoder_id == connector->encoder_id)||(encoder->encoder_id==0))
		{
			printf("using connector->encoders[%d]\n",i);
			printf("connector->encoders[%d]->encoder_id %d\n",i,encoder->encoder_id);
			printf("connector->encoders[%d]->crtc_id %d\n",i,encoder->crtc_id);
			printf("connector->encoders[%d]->possible_crtcs %d\n",i,encoder->possible_crtcs);
			break;
		}
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		crtc = drmModeGetCrtc(fd, resources->crtcs[i]);

		if (crtc == NULL)
		continue;

		if((1<<i)&encoder->possible_crtcs)
		{
			printf("using resources->crtcs[%d]\n",i);
			printf("resources->crtcs[%d]->x %d\n",i,crtc->x);
			printf("resources->crtcs[%d]->y %d\n",i,crtc->y);
			printf("resources->crtcs[%d]->mode_valid %d\n",i,crtc->mode_valid);
			kms->crtc_id = crtc->crtc_id;

			break;
		}
		drmModeFreeCrtc(crtc);
	}

	if(encoder->crtc_id)
	{
		kms->crtc_id = encoder->crtc_id;
	}

	kms->connector = connector;
	kms->encoder = encoder;
	kms->mode = connector->modes[0];

	return EGL_TRUE;
}

static void
render_stuff(int width, int height)
{
	glViewport(0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, width, 0, height, 1.0, -1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0f, 0.0f, 0.0f);

	glRectf(x, y, x + rsize, y + rsize);

	if (x <= 0 || x >= width - rsize)
	 xstep *= -1;

	if (y <= 0 || y >= height - rsize)
	 ystep *= -1;

	x += xstep;
	y += ystep;
}

static const char device_name[] = "/dev/dri/card0";

static const EGLint attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 1,
	EGL_GREEN_SIZE, 1,
	EGL_BLUE_SIZE, 1,
	EGL_ALPHA_SIZE, 0,
	EGL_DEPTH_SIZE, 1,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

void quit_handler(int signum)
{
	quit = 1;
	printf("Quitting!\n");
}

uint32_t current_fb_id, next_fb_id;
struct gbm_bo *current_bo, *next_bo;
struct gbm_surface *gs;
struct kms kms;
struct gbm_bo *test_bo;

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	if (current_fb_id)
		drmModeRmFB(fd, current_fb_id);
	current_fb_id = next_fb_id;
	next_fb_id = 0;

	if (current_bo)
		gbm_surface_release_buffer(gs, current_bo);
	current_bo = next_bo;
	next_bo = NULL;
}

int main(int argc, char *argv[])
{
	EGLDisplay dpy;
	EGLContext ctx;
	EGLConfig config;
	EGLSurface surface;
	EGLint major, minor, n;
	const char *ver;
	uint32_t handle, stride;
	int ret, fd, frames = 0;
	struct gbm_device *gbm;
	drmModeCrtcPtr saved_crtc;
	time_t start, end;
	char *data;
	char j;
	int i;
	int once;
	once = 0;

	signal (SIGINT, quit_handler);

	fd = open(device_name, O_RDWR);
	if (fd < 0) {
		/* Probably permissions error */
		fprintf(stderr, "couldn't open %s, skipping\n", device_name);
		return -1;
	}

	gbm = gbm_create_device(fd);
	if (gbm == NULL) {
		fprintf(stderr, "couldn't create gbm device\n");
		ret = -1;
		goto close_fd;
	}

	dpy = eglGetDisplay(gbm);
	if (dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetDisplay() failed\n");
		ret = -1;
		goto destroy_gbm_device;
	}

	if (!eglInitialize(dpy, &major, &minor)) {
		printf("eglInitialize() failed\n");
		ret = -1;
		goto egl_terminate;
	}

	ver = eglQueryString(dpy, EGL_VERSION);
	printf("EGL_VERSION = %s\n", ver);

	if (!setup_kms(fd, &kms)) {
		ret = -1;
		goto egl_terminate;
	}

	eglBindAPI(EGL_OPENGL_API);

	if (!eglChooseConfig(dpy, attribs, &config, 1, &n) || n != 1) {
		fprintf(stderr, "failed to choose argb config\n");
		goto egl_terminate;
	}

	ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
	if (ctx == NULL) {
		fprintf(stderr, "failed to create context\n");
		ret = -1;
		goto egl_terminate;
	}

	gs = gbm_surface_create(gbm, kms.mode.hdisplay, kms.mode.vdisplay,
		GBM_BO_FORMAT_XRGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (gs == NULL) {
		fprintf(stderr, "unable to create gbm surface\n");
		ret = -1;
		goto egl_terminate;
	}


	surface = eglCreateWindowSurface(dpy, config, gs, NULL);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create surface\n");
		ret = -1;
		goto destroy_gbm_surface;
	}

	if (!eglMakeCurrent(dpy, surface, surface, ctx)) {
		fprintf(stderr, "failed to make context current\n");
		ret = -1;
		goto destroy_surface;
	}

	saved_crtc = drmModeGetCrtc(fd, kms.crtc_id);
	if (saved_crtc == NULL)
	{
		fprintf(stderr, "no valid graphic configuration active (VT ?)\n");
	}
	time(&start);
	do {

		drmEventContext evctx;
		fd_set rfds;

		render_stuff(kms.mode.hdisplay, kms.mode.vdisplay);
		eglSwapBuffers(dpy, surface);
		
		if (!gbm_surface_has_free_buffers(gs))
			fprintf(stderr, "out of free buffers\n");

		next_bo = gbm_surface_lock_front_buffer(gs);
		if (!next_bo)
			fprintf(stderr, "failed to lock front buffer: %m\n");

		handle = gbm_bo_get_handle(next_bo).u32;
		stride = gbm_bo_get_stride(next_bo);
		
		ret = drmModeAddFB(fd,
				 kms.mode.hdisplay, kms.mode.vdisplay,
				 24, 32, stride, handle, &next_fb_id);
		if (ret) {
			fprintf(stderr, "failed to create fb\n");
			goto out;
		}
		  
		/* make sure to setup crtc once (fix for broken drivers) */
		if(once == 0){
			once = 1;
			drmModeSetCrtc(fd, kms.crtc_id, next_fb_id,
				0, 0,
				&kms.connector->connector_id, 1, &kms.mode);
		}
		
		ret = drmModePageFlip(fd, kms.crtc_id,
					next_fb_id,
					DRM_MODE_PAGE_FLIP_EVENT, 0);
		if (ret) {
			fprintf(stderr, "failed to page flip: %m\n");
			goto out;
		}

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		while (select(fd + 1, &rfds, NULL, NULL, NULL) == -1)
			NULL;

		memset(&evctx, 0, sizeof evctx);
		evctx.version = DRM_EVENT_CONTEXT_VERSION;
		evctx.page_flip_handler = page_flip_handler;

		drmHandleEvent(fd, &evctx);

		frames++;
	} while (!quit);
	time(&end);

	printf("Frames per second: %.2lf\n", frames / difftime(end, start));

out:
	if(saved_crtc){
		drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
			saved_crtc->x, saved_crtc->y,
			&kms.connector->connector_id, 1, &saved_crtc->mode);
		}
	drmModeFreeCrtc(saved_crtc);
	if (current_fb_id)
		drmModeRmFB(fd, current_fb_id);
	if (next_fb_id)
		drmModeRmFB(fd, next_fb_id);
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
destroy_context:
	eglDestroyContext(dpy, ctx);
destroy_surface:
	eglDestroySurface(dpy, surface);
destroy_gbm_surface:
	gbm_surface_destroy(gs);
egl_terminate:
	eglTerminate(dpy);
destroy_gbm_device:
	gbm_device_destroy(gbm);
close_fd:
	close(fd);

	return ret;
}
