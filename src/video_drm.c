/* -*- Mode: c; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "video_drm.h"

static const char *s_card_paths[] = {
    "/dev/dri/card1",
    "/dev/dri/card0",
    "/dev/dri/card2",
    NULL
};

static int FindConnectedConnector(int fd, drmModeRes *res,
                                  drmModeConnector **out_conn,
                                  drmModeModeInfo *out_mode,
                                  uint32_t *out_crtc_id,
                                  drmModeEncoder **out_enc)
{
    int i, j;
    drmModeConnector *conn = NULL;

    for (i = 0; i < res->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (c) {
            if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
                conn = c;
                break;
            }
            drmModeFreeConnector(c);
        }
    }

    if (!conn) {
        return 1;
    }

    drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
    uint32_t crtc_id = 0;
    drmModeModeInfo mode;
    int have_mode = 0;

    if (enc) {
        if (enc->crtc_id) {
            crtc_id = enc->crtc_id;
            drmModeCrtc *c = drmModeGetCrtc(fd, crtc_id);
            if (c) {
                if (c->mode_valid) {
                    mode = c->mode;
                    have_mode = 1;
                }
                drmModeFreeCrtc(c);
            }
        }
        if (!crtc_id) {
            for (j = 0; j < res->count_crtcs; j++) {
                if (enc->possible_crtcs & (1 << j)) {
                    crtc_id = res->crtcs[j];
                    break;
                }
            }
        }
    }

    if (!have_mode) {
        /* Choose mode with highest resolution */
        int best_area = 0;
        mode = conn->modes[0];
        for (i = 0; i < conn->count_modes; i++) {
            int area = conn->modes[i].hdisplay * conn->modes[i].vdisplay;
            if (area > best_area) {
                best_area = area;
                mode = conn->modes[i];
            }
        }
    }

    if (crtc_id == 0 && res->count_crtcs > 0) {
        crtc_id = res->crtcs[0];
    }

    *out_conn = conn;
    *out_mode = mode;
    *out_crtc_id = crtc_id;
    if (out_enc) {
        *out_enc = enc;
    } else if (enc) {
        drmModeFreeEncoder(enc);
    }
    return 0;
}

int VideoDRM_GetScreenResolution(int *out_width, int *out_height)
{
    int i;
    for (i = 0; s_card_paths[i]; i++) {
        int fd = open(s_card_paths[i], O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        drmModeRes *res = drmModeGetResources(fd);
        if (!res) {
            close(fd);
            continue;
        }
        drmModeConnector *conn = NULL;
        drmModeModeInfo mode;
        uint32_t crtc_id = 0;
        if (FindConnectedConnector(fd, res, &conn, &mode, &crtc_id, NULL) == 0) {
            *out_width = mode.hdisplay;
            *out_height = mode.vdisplay;
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            close(fd);
            return 0;
        }
        drmModeFreeResources(res);
        close(fd);
    }
    return 1;
}

int VideoDRM_Init(VideoDRM *drm)
{
    int i;
    memset(drm, 0, sizeof(*drm));
    drm->fd = -1;

    for (i = 0; s_card_paths[i]; i++) {
        int fd = open(s_card_paths[i], O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }

        drmModeRes *res = drmModeGetResources(fd);
        if (!res) {
            close(fd);
            continue;
        }

        drmModeConnector *conn = NULL;
        drmModeModeInfo mode;
        uint32_t crtc_id = 0;
        drmModeEncoder *enc = NULL;

        if (FindConnectedConnector(fd, res, &conn, &mode, &crtc_id, &enc) != 0) {
            drmModeFreeResources(res);
            close(fd);
            continue;
        }

        /* Check if we can obtain or hold DRM master */
        if (drmSetMaster(fd) != 0) {
            fprintf(stderr, "[drm] Notice: drmSetMaster failed on %s (another display server active, falling back to X11)\r\n", s_card_paths[i]);
            if (enc) drmModeFreeEncoder(enc);
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            close(fd);
            continue;
        }

        drmModeCrtc *saved_crtc = drmModeGetCrtc(fd, crtc_id);
        struct gbm_device *gbm = gbm_create_device(fd);
        if (!gbm) {
            if (saved_crtc) drmModeFreeCrtc(saved_crtc);
            if (enc) drmModeFreeEncoder(enc);
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            close(fd);
            continue;
        }

        drm->fd = fd;
        drm->resources = res;
        drm->connector = conn;
        drm->encoder = enc;
        drm->mode = mode;
        drm->crtc_id = crtc_id;
        drm->saved_crtc = saved_crtc;
        drm->gbm = gbm;
        drm->is_active = 0;

        printf("[drm] Initialized hardware KMS on %s: mode %dx%d @ %dHz (CRTC %u, Conn %u)\r\n",
               s_card_paths[i], mode.hdisplay, mode.vdisplay, mode.vrefresh, crtc_id, conn->connector_id);
        return 0;
    }

    return 1;
}

int VideoDRM_CreateSurface(VideoDRM *drm, int width, int height)
{
    if (!drm->gbm) {
        return 1;
    }

    if (width <= 0) width = drm->mode.hdisplay;
    if (height <= 0) height = drm->mode.vdisplay;

    drm->gbm_surface = gbm_surface_create(drm->gbm, width, height,
                                          GBM_FORMAT_XRGB8888,
                                          GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!drm->gbm_surface) {
        fprintf(stderr, "[drm] gbm_surface_create failed (%dx%d)\r\n", width, height);
        return 2;
    }

    return 0;
}

int VideoDRM_SwapBuffers(VideoDRM *drm, EGLDisplay display, EGLSurface surface)
{
    if (!drm->gbm_surface || drm->fd < 0) {
        return 1;
    }

    eglSwapBuffers(display, surface);

    struct gbm_bo *bo = gbm_surface_lock_front_buffer(drm->gbm_surface);
    if (!bo) {
        return 2;
    }

    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t fb_id = 0;

    int ret = drmModeAddFB(drm->fd, width, height, 24, 32, stride, handle, &fb_id);
    if (ret != 0) {
        gbm_surface_release_buffer(drm->gbm_surface, bo);
        return 3;
    }

    if (!drm->is_active) {
        ret = drmModeSetCrtc(drm->fd, drm->crtc_id, fb_id, 0, 0,
                             &drm->connector->connector_id, 1, &drm->mode);
        if (ret != 0) {
            /* If CRTC set fails (e.g. permission/master issue), report error */
            drmModeRmFB(drm->fd, fb_id);
            gbm_surface_release_buffer(drm->gbm_surface, bo);
            return 4;
        }
        drm->is_active = 1;
    } else {
        ret = drmModePageFlip(drm->fd, drm->crtc_id, fb_id, 0, NULL);
        if (ret != 0) {
            /* Fall back to synchronous SetCrtc */
            drmModeSetCrtc(drm->fd, drm->crtc_id, fb_id, 0, 0,
                           &drm->connector->connector_id, 1, &drm->mode);
        }
    }

    if (drm->prev_bo) {
        drmModeRmFB(drm->fd, drm->prev_fb_id);
        gbm_surface_release_buffer(drm->gbm_surface, drm->prev_bo);
    }

    drm->prev_bo = bo;
    drm->prev_fb_id = fb_id;
    return 0;
}

void VideoDRM_Cleanup(VideoDRM *drm)
{
    if (drm->saved_crtc && drm->fd >= 0) {
        drmModeSetCrtc(drm->fd, drm->saved_crtc->crtc_id, drm->saved_crtc->buffer_id,
                       drm->saved_crtc->x, drm->saved_crtc->y,
                       &drm->connector->connector_id, 1, &drm->saved_crtc->mode);
        drmModeFreeCrtc(drm->saved_crtc);
        drm->saved_crtc = NULL;
    }

    if (drm->prev_bo && drm->gbm_surface && drm->fd >= 0) {
        drmModeRmFB(drm->fd, drm->prev_fb_id);
        gbm_surface_release_buffer(drm->gbm_surface, drm->prev_bo);
        drm->prev_bo = NULL;
        drm->prev_fb_id = 0;
    }

    if (drm->gbm_surface) {
        gbm_surface_destroy(drm->gbm_surface);
        drm->gbm_surface = NULL;
    }

    if (drm->gbm) {
        gbm_device_destroy(drm->gbm);
        drm->gbm = NULL;
    }

    if (drm->encoder) {
        drmModeFreeEncoder(drm->encoder);
        drm->encoder = NULL;
    }

    if (drm->connector) {
        drmModeFreeConnector(drm->connector);
        drm->connector = NULL;
    }

    if (drm->resources) {
        drmModeFreeResources(drm->resources);
        drm->resources = NULL;
    }

    if (drm->fd >= 0) {
        drmDropMaster(drm->fd);
        close(drm->fd);
        drm->fd = -1;
    }

    drm->is_active = 0;
}
