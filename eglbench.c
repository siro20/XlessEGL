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
GLfloat rsize = 100;

int quit = 0;

GLint tex;
uint8_t *data[12];

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

static int has_extension(EGLDisplay dpy, char *str)
{
	const char *ext = eglQueryString(dpy, EGL_EXTENSIONS);
	if(ext && str && strlen(ext) && strlen(str))
		if(strstr(ext, str))
			return 1;
	ext = glGetString(GL_EXTENSIONS);
	if(ext && str && strlen(ext) && strlen(str))
		if(strstr(ext, str))
			return 1;
	return 0;
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
	
	glBindTexture(GL_TEXTURE_2D, tex);
	
	x = 0;
	y = 0;
	
	GLfloat vtx1[] = {
		x, y, 0,
		x+width, y, 0,
		x+width, y+height, 0,
		x, y+height, 0
	};
	static const GLfloat tex1[] = {
		0,0,
		1,0,
		1,1,
		0,1
	};

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, vtx1);
	glTexCoordPointer(2, GL_FLOAT, 0, tex1);
	glDrawArrays(GL_TRIANGLE_FAN,0,4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindTexture(GL_TEXTURE_2D, 0);
}

static void waitforGLcompletion(struct timeval t1, EGLDisplay dpy){
	int ret;
	struct timeval t2;

	if(has_extension(dpy, "EGL_KHR_fence_sync") || has_extension(dpy, "GL_OES_EGL_sync") || has_extension(dpy, "VG_KHR_EGL_sync")){
		EGLSyncKHR eglSyncObj = eglCreateSyncKHR(dpy, EGL_SYNC_FENCE_KHR, NULL);
		if (eglSyncObj != EGL_NO_SYNC_KHR) {
			ret = eglClientWaitSyncKHR(dpy,eglSyncObj,EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,EGL_FOREVER_KHR);
			if (ret != EGL_FALSE) {
				gettimeofday(&t2, NULL);
				eglGetSyncAttribKHR(dpy,eglSyncObj,EGL_SYNC_STATUS_KHR,&ret);
				if(ret == EGL_SIGNALED_KHR) {
					printf("took %d usec\n", (t2.tv_usec + t2.tv_sec * 1000000)-(t1.tv_usec + t1.tv_sec * 1000000));
				}
				else
				{
					fprintf(stderr, "eglGetSyncAttribKHR error waiting for fence: %#x\n",eglGetError());
				}
			}
			else
			{
				fprintf(stderr, "eglClientWaitSyncKHR error waiting for fence: %#x\n",eglGetError());
			}

		}
		else
		{
			fprintf(stderr, "eglCreateSyncKHR error creating fence: %#x\n", eglGetError());
		}
	}
	else if(has_extension(dpy, "GL_ARB_sync")){
		GLsync eglSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		ret = eglGetError();
		if ( eglSyncObj != NULL && ret != GL_INVALID_ENUM  && ret != GL_INVALID_VALUE ){
			gettimeofday(&t1, NULL);
			glClientWaitSync(eglSyncObj, 0, 1000000000LL);
			gettimeofday(&t2, NULL);
			printf("took %d usec\n", (t2.tv_usec + t2.tv_sec * 1000000)-(t1.tv_usec + t1.tv_sec * 1000000));
		}
		else
		{
			fprintf(stderr, "glFenceSync failed.\n");
		}
	}
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

static uint16_t _log2(uint32_t n) {
	uint16_t logValue = -1;
	while (n) {
		logValue++;
		n >>= 1;
	}
	return logValue;
}

static void test_GL_ARB_pixel_buffer_object(EGLDisplay dpy, int z){
	int i,j;
	char *tmp;

	if(has_extension(dpy,"GL_ARB_pixel_buffer_object")){
		GLuint pboID;
		struct timeval t1;
		
		glGenBuffersARB(1, &pboID);
		
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboID);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, z * z * 4, 0, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, z, z, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		/* call after glTexImage2D !!!! */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 
		
		gettimeofday(&t1, NULL);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboID);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, z*z*4, 0, GL_STREAM_DRAW_ARB);
		tmp = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(!tmp)
		{
			fprintf( stderr, "ERROR: glMapBufferARB failed.\n" );
			return;
		}
		memcpy(tmp, data[_log2(z)-1], z * z * 4);
		
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, z, z, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

		printf("testing surface with dimension (%dx%d): ", z, z );
		waitforGLcompletion(t1, dpy);
		glFlush();

		glDeleteBuffersARB(1, &pboID);
		glBindTexture(GL_TEXTURE_2D, 0);

	}
}

static void test_glDrawPixels(EGLDisplay dpy, int z, int width, int height, int k)
{
	int i,j;
	struct timeval t1;
	
	gettimeofday(&t1, NULL);
	if(k == 0)
		printf("testing surface with dimension (%dx%d): ", z, z );
	glViewport(0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, width, 0, height, 1.0, -1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2i(0,0);

	glDrawPixels(z, z,  GL_RGBA, GL_UNSIGNED_BYTE, data[_log2(z)-1]);
	if (eglGetError() != GL_NO_ERROR) {
		if(k == 0)
			waitforGLcompletion(t1, dpy);
	}
	else
	{
		fprintf(stderr, "glDrawPixels failed with %#x\n", eglGetError());
		return;
	}
	glFlush();
}

static void test_glTexSubImage2D(EGLDisplay dpy, int z)
{
	int i,j;
	struct timeval t1;
	
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, z, z, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); 
	/* call after glTexImage2D !!!! */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	gettimeofday(&t1, NULL);  
	printf("testing surface with dimension (%dx%d): ", z, z );
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, z, z, GL_RGBA, GL_UNSIGNED_BYTE, data[_log2(z)-1]);
	if (eglGetError() != GL_NO_ERROR) {
		waitforGLcompletion(t1, dpy);
	}
	else
	{
		fprintf(stderr, "glTexSubImage2D failed with %#x\n", eglGetError());
		return;
	}
	glFlush();
	glBindTexture(GL_TEXTURE_2D, 0);
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
	int i,j,k,z;
	int once = 0;
	drmModeCrtcPtr saved_crtc;

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

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	if (glGetError() != GL_NO_ERROR) {
		fprintf(stderr, "Could not create/bind texture");
		ret = -1;
		goto destroy_surface;
	}
	// Upload the content of the bitmap in the GraphicBuffer
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	// just add the colors, do not use alpha
	glBlendFunc(GL_ONE, GL_ONE);
	// background color, set blue channel to 50%
	glClearColor(0.0f, 0.0f, 0.5f, 1.0f);  

	k = 0;
	for(z=2;z<=2048;z*=2)
	{
		uint8_t *tmp = malloc(z * z * 4);
		for(j=0; j<z; j++)
		{
			for(i=0; i<z; i++)
			{
				tmp[((j*(int)z)+i)*4] = i & 0x10 ? 0xff : 0;
				tmp[((j*(int)z)+i)*4+1] = j & 0x10 ? 0xff : 0;
				tmp[((j*(int)z)+i)*4+2] = (i+j) & 0x10 ? 0xff : 0;
				tmp[((j*(int)z)+i)*4+3] = 0xff;
			}
		}
		data[k] = tmp;
		k++;
	}
	saved_crtc = drmModeGetCrtc(fd, kms.crtc_id);
	if (saved_crtc == NULL)
	{
		fprintf(stderr, "no valid graphic configuration active (VT ?)\n");
	}
	j = 0;
	i = 2;
	k = 0;
	printf("***glDrawPixels***\n");
	do {
		drmEventContext evctx;
		fd_set rfds;

		k++;
		if(k == 20)
		{
			i*=2;
			k = 0;
			if(i > 2048)
			{
				i = 2;
				j++;
				if(j>2)
				{
					goto out;
				}
				if(j==1)
					printf("***glTexSubImage2D***\n");
				if(j==2)
					printf("***GL_ARB_pixel_buffer_object***\n");
			}
			if(j==1)
				test_glTexSubImage2D(dpy, i);
			if(j==2)
				test_GL_ARB_pixel_buffer_object(dpy, i);
		}
		if(j != 0)
			render_stuff(kms.mode.hdisplay, kms.mode.vdisplay);
		else
			test_glDrawPixels(dpy, i, kms.mode.hdisplay, kms.mode.vdisplay, k);
		
		//eglSwapBuffers crash without any opengl command issued !
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
	} while (!quit);

out:
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
