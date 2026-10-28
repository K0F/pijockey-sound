/* -*- Mode: c; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#ifndef INCLUDED_VIDEO_DRM_H
#define INCLUDED_VIDEO_DRM_H

#include <stddef.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>

typedef struct {
    int fd;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    drmModeCrtc *saved_crtc;
    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;
    struct gbm_bo *prev_bo;
    uint32_t prev_fb_id;
    int is_active;
} VideoDRM;

int VideoDRM_GetScreenResolution(int *out_width, int *out_height);
int VideoDRM_Init(VideoDRM *drm);
void VideoDRM_Cleanup(VideoDRM *drm);
int VideoDRM_CreateSurface(VideoDRM *drm, int width, int height);
int VideoDRM_SwapBuffers(VideoDRM *drm, EGLDisplay display, EGLSurface surface);

#endif
