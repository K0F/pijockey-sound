/* -*- Mode: c; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <bcm_host.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"
#include "base.h"
#include "video_drm.h"
#include "video_egl.h"

typedef struct {
    DISPMANX_ELEMENT_HANDLE_T element;
    int width;
    int height;
} EGL_DISPMANX_WINDOW_T;

typedef enum {
    BACKEND_NONE = 0,
    BACKEND_DRM,
    BACKEND_X11,
    BACKEND_DISPMANX
} VideoBackend;

struct VideoEGL_ {
    VideoBackend backend;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    VideoDRM drm;
    Display *x11_display;
    Window x11_window;
    EGL_DISPMANX_WINDOW_T native_window;
};

size_t VideoEGL_InstanceSize(void)
{
    return sizeof(VideoEGL);
}

int VideoEGL_Construct(VideoEGL *ve)
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config;
    EGLBoolean r;
    int backend_chosen = BACKEND_NONE;

    memset(ve, 0, sizeof(*ve));

    /* 1. Prefer native DRM/KMS + GBM directly on the hardware */
    if (VideoDRM_Init(&ve->drm) == 0) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
            (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (get_platform_display) {
            display = get_platform_display(EGL_PLATFORM_GBM_KHR, ve->drm.gbm, NULL);
        }
        if (display == EGL_NO_DISPLAY) {
            display = eglGetDisplay((EGLNativeDisplayType)ve->drm.gbm);
        }

        if (display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL) == EGL_TRUE) {
            backend_chosen = BACKEND_DRM;
            printf("[video] Native DRM/KMS + GBM backend activated\r\n");
        } else {
            if (display != EGL_NO_DISPLAY) {
                eglTerminate(display);
                display = EGL_NO_DISPLAY;
            }
            VideoDRM_Cleanup(&ve->drm);
        }
    }

    /* 2. Fallback to X11 display if DRM is unavailable or DRM master locked by Xorg */
    if (backend_chosen == BACKEND_NONE) {
        const char *disp_name = getenv("DISPLAY");
        if (!disp_name || disp_name[0] == '\0') {
            disp_name = ":0";
        }

        if (!getenv("XAUTHORITY")) {
            const char *home = getenv("HOME");
            if (home) {
                char auth[512];
                snprintf(auth, sizeof(auth), "%s/.Xauthority", home);
                if (access(auth, R_OK) == 0) {
                    setenv("XAUTHORITY", auth, 0);
                }
            }
        }

        ve->x11_display = XOpenDisplay(disp_name);
        if (!ve->x11_display && strcmp(disp_name, ":0") != 0) {
            ve->x11_display = XOpenDisplay(":0");
        }

        if (ve->x11_display) {
            display = eglGetDisplay((EGLNativeDisplayType)ve->x11_display);
            if (display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL) == EGL_TRUE) {
                backend_chosen = BACKEND_X11;
                printf("[video] X11 display backend activated\r\n");
            }
        }
    }

    /* 3. Fallback to legacy default EGL display (e.g. Dispmanx on old firmware) */
    if (backend_chosen == BACKEND_NONE) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL) == EGL_TRUE) {
            backend_chosen = BACKEND_DISPMANX;
            printf("[video] Default EGL backend activated\r\n");
        }
    }

    if (backend_chosen == BACKEND_NONE || display == EGL_NO_DISPLAY) {
        fprintf(stderr, "VideoEGL_Construct: Failed to initialize any EGL display backend\r\n");
        return 1;
    }

    ve->backend = backend_chosen;
    ve->display = display;

    {
        EGLint num;
        static const EGLint config_attrib_list[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        r = eglChooseConfig(display, config_attrib_list, &config, 1, &num);
        if (r != EGL_TRUE || num < 1) {
            fprintf(stderr, "VideoEGL_Construct: eglChooseConfig failed\r\n");
            if (ve->backend == BACKEND_DRM) VideoDRM_Cleanup(&ve->drm);
            eglTerminate(display);
            return 5;
        }
    }

    {
        static const EGLint context_attrib_list[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib_list);
        if (context == EGL_NO_CONTEXT) {
            fprintf(stderr, "VideoEGL_Construct: eglCreateContext failed\r\n");
            if (ve->backend == BACKEND_DRM) VideoDRM_Cleanup(&ve->drm);
            eglTerminate(display);
            return 6;
        }
    }

    ve->context = context;
    ve->surface = EGL_NO_SURFACE;
    ve->config = config;
    return 0;
}

void VideoEGL_Destruct(VideoEGL *ve)
{
    VideoEGL_DestroySurface(ve);
    if (ve->context) {
        eglDestroyContext(ve->display, ve->context);
        ve->context = 0;
    }
    if (ve->display) {
        eglTerminate(ve->display);
        ve->display = 0;
    }
    if (ve->backend == BACKEND_DRM) {
        VideoDRM_Cleanup(&ve->drm);
    }
    if (ve->x11_display) {
        XCloseDisplay(ve->x11_display);
        ve->x11_display = NULL;
    }
    memset(ve, 0, sizeof(*ve));
}

int VideoEGL_CreateSurface(VideoEGL *ve, unsigned int native_window_element, int width, int height)
{
    if (ve->backend == BACKEND_DRM) {
        if (VideoDRM_CreateSurface(&ve->drm, width, height) != 0) {
            return 1;
        }
        ve->surface = eglCreateWindowSurface(ve->display, ve->config,
                                             (EGLNativeWindowType)ve->drm.gbm_surface, NULL);
    } else if (ve->backend == BACKEND_X11 && ve->x11_display) {
        int screen = DefaultScreen(ve->x11_display);
        Window root = RootWindow(ve->x11_display, screen);
        EGLint visual_id = 0;
        eglGetConfigAttrib(ve->display, ve->config, EGL_NATIVE_VISUAL_ID, &visual_id);

        if (visual_id != 0) {
            XVisualInfo vinfo_template;
            int num_visuals = 0;
            vinfo_template.visualid = visual_id;
            XVisualInfo *vinfo = XGetVisualInfo(ve->x11_display, VisualIDMask, &vinfo_template, &num_visuals);
            if (vinfo) {
                Colormap colormap = XCreateColormap(ve->x11_display, root, vinfo->visual, AllocNone);
                XSetWindowAttributes swa;
                swa.colormap = colormap;
                swa.background_pixmap = None;
                swa.border_pixel = 0;
                swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
                ve->x11_window = XCreateWindow(ve->x11_display, root, 0, 0, width, height, 0,
                                               vinfo->depth, InputOutput, vinfo->visual,
                                               CWBorderPixel | CWColormap | CWEventMask, &swa);
                XFree(vinfo);
            }
        }
        if (!ve->x11_window) {
            ve->x11_window = XCreateSimpleWindow(ve->x11_display, root, 0, 0, width, height, 0, 0, 0);
        }
        XStoreName(ve->x11_display, ve->x11_window, "PiJockey");
        XMapWindow(ve->x11_display, ve->x11_window);
        XFlush(ve->x11_display);
        ve->surface = eglCreateWindowSurface(ve->display, ve->config, (EGLNativeWindowType)ve->x11_window, NULL);
    } else {
        EGL_DISPMANX_WINDOW_T *nw;
        nw = &ve->native_window;
        nw->element = (DISPMANX_ELEMENT_HANDLE_T)native_window_element;
        nw->width = width;
        nw->height = height;
        ve->surface = eglCreateWindowSurface(ve->display, ve->config, (EGLNativeWindowType)nw, NULL);
    }

    if (ve->surface == EGL_NO_SURFACE) {
        fprintf(stderr, "VideoEGL_CreateSurface failed (error 0x%x)\r\n", eglGetError());
        return 1;
    }
    return 0;
}

void VideoEGL_DestroySurface(VideoEGL *ve)
{
    if (ve->surface) {
        eglDestroySurface(ve->display, ve->surface);
        ve->surface = 0;
    }
    if (ve->x11_display && ve->x11_window) {
        XDestroyWindow(ve->x11_display, ve->x11_window);
        ve->x11_window = 0;
    }
}

int VideoEGL_MakeCurrent(VideoEGL *ve)
{
    return (eglMakeCurrent(ve->display, ve->surface, ve->surface, ve->context) == EGL_TRUE) ? 0 : 1;
}

int VideoEGL_UnmakeCurrent(VideoEGL *ve)
{
    eglMakeCurrent(ve->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return 0;
}

int VideoEGL_SwapBuffers(VideoEGL *ve)
{
    if (ve->backend == BACKEND_DRM) {
        return VideoDRM_SwapBuffers(&ve->drm, ve->display, ve->surface);
    }

    EGLBoolean r = eglSwapBuffers(ve->display, ve->surface);
    if (ve->x11_display) {
        XFlush(ve->x11_display);
    }
    return (r == EGL_TRUE) ? 0 : 1;
}
