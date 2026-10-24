#include "bcm_host.h"
#include <stddef.h>

void bcm_host_init(void) {}
void bcm_host_deinit(void) {}

int32_t graphics_get_display_size(const uint16_t device_number, uint32_t *width, uint32_t *height) {
    (void)device_number;
    if (width) *width = 1920;
    if (height) *height = 1080;
    return 0;
}

DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update_start(int32_t priority) {
    (void)priority;
    return 1;
}

int vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T update) {
    (void)update;
    return 0;
}

void vc_dispmanx_rect_set(VC_RECT_T *rect, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (rect) {
        rect->x = x;
        rect->y = y;
        rect->width = width;
        rect->height = height;
    }
}

DISPMANX_DISPLAY_HANDLE_T vc_dispmanx_display_open(uint32_t device) {
    (void)device;
    return 1;
}

int vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T display) {
    (void)display;
    return 0;
}

DISPMANX_ELEMENT_HANDLE_T vc_dispmanx_element_add(DISPMANX_UPDATE_HANDLE_T update,
                                                  DISPMANX_DISPLAY_HANDLE_T display,
                                                  int32_t layer,
                                                  const VC_RECT_T *dest_rect,
                                                  DISPMANX_RESOURCE_HANDLE_T src,
                                                  const VC_RECT_T *src_rect,
                                                  int protection,
                                                  VC_DISPMANX_ALPHA_T *alpha,
                                                  DISPMANX_CLAMP_T *clamp,
                                                  int transform) {
    (void)update;
    (void)display;
    (void)layer;
    (void)dest_rect;
    (void)src;
    (void)src_rect;
    (void)protection;
    (void)alpha;
    (void)clamp;
    (void)transform;
    return 1;
}

int vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element) {
    (void)update;
    (void)element;
    return 0;
}

int vc_dispmanx_element_change_attributes(DISPMANX_UPDATE_HANDLE_T update,
                                          DISPMANX_ELEMENT_HANDLE_T element,
                                          uint32_t change_flags,
                                          int32_t layer,
                                          uint8_t opacity,
                                          const VC_RECT_T *dest_rect,
                                          const VC_RECT_T *src_rect,
                                          DISPMANX_RESOURCE_HANDLE_T mask,
                                          int transform) {
    (void)update;
    (void)element;
    (void)change_flags;
    (void)layer;
    (void)opacity;
    (void)dest_rect;
    (void)src_rect;
    (void)mask;
    (void)transform;
    return 0;
}
