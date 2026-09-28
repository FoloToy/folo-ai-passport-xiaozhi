#ifndef LVGL_SCREEN_ROUNDING_H
#define LVGL_SCREEN_ROUNDING_H

#include <stdbool.h>
#include <stdint.h>

// AI Passport's physical screen has a 30 px final-screen radius. The mask is
// applied to the RGB565 flush buffer so the C3 does not need an ARGB layer.
#define BSP_LVGL_SCREEN_RADIUS 30

#ifdef __cplusplus
extern "C" {
#endif

bool bsp_display_rounded_row_span(int32_t y, int32_t width, int32_t height,
                                  int32_t radius, int32_t* x1, int32_t* x2);

#ifdef __cplusplus
}
#endif

#endif  // LVGL_SCREEN_ROUNDING_H
