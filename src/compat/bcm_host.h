#ifndef BCM_HOST_H
#define BCM_HOST_H

#include <stdint.h>
#include <sched.h>

#define DISPMANX_NO_HANDLE 0
#define DISPMANX_PROTECTION_NONE 0
#define DISPMANX_NO_ROTATE 0

typedef uint32_t DISPMANX_DISPLAY_HANDLE_T;
typedef uint32_t DISPMANX_ELEMENT_HANDLE_T;
typedef uint32_t DISPMANX_UPDATE_HANDLE_T;
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;
typedef void VC_DISPMANX_ALPHA_T;
typedef void DISPMANX_CLAMP_T;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} VC_RECT_T;

void bcm_host_init(void);
void bcm_host_deinit(void);
int32_t graphics_get_display_size(const uint16_t device_number, uint32_t *width, uint32_t *height);
DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update_start(int32_t priority);
int vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T update);
void vc_dispmanx_rect_set(VC_RECT_T *rect, int32_t x, int32_t y, int32_t width, int32_t height);
DISPMANX_DISPLAY_HANDLE_T vc_dispmanx_display_open(uint32_t device);
int vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T display);
DISPMANX_ELEMENT_HANDLE_T vc_dispmanx_element_add(DISPMANX_UPDATE_HANDLE_T update,
                                                  DISPMANX_DISPLAY_HANDLE_T display,
                                                  int32_t layer,
                                                  const VC_RECT_T *dest_rect,
                                                  DISPMANX_RESOURCE_HANDLE_T src,
                                                  const VC_RECT_T *src_rect,
                                                  int protection,
                                                  VC_DISPMANX_ALPHA_T *alpha,
                                                  DISPMANX_CLAMP_T *clamp,
                                                  int transform);
int vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element);
int vc_dispmanx_element_change_attributes(DISPMANX_UPDATE_HANDLE_T update,
                                          DISPMANX_ELEMENT_HANDLE_T element,
                                          uint32_t change_flags,
                                          int32_t layer,
                                          uint8_t opacity,
                                          const VC_RECT_T *dest_rect,
                                          const VC_RECT_T *src_rect,
                                          DISPMANX_RESOURCE_HANDLE_T mask,
                                          int transform);

#endif /* BCM_HOST_H */
