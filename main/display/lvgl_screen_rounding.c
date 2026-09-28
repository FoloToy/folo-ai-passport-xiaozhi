#include "lvgl_screen_rounding.h"

static int32_t clamp_radius(int32_t width, int32_t height, int32_t radius) {
    if (radius <= 0 || width <= 0 || height <= 0) {
        return 0;
    }
    const int32_t max_radius = (width < height ? width : height) / 2;
    return radius > max_radius ? max_radius : radius;
}

bool bsp_display_rounded_row_span(int32_t y, int32_t width, int32_t height,
                                  int32_t radius, int32_t* x1, int32_t* x2) {
    if (!x1 || !x2 || width <= 0 || height <= 0 || y < 0 || y >= height) {
        return false;
    }
    radius = clamp_radius(width, height, radius);
    if (radius <= 0 || (y >= radius && y < height - radius)) {
        *x1 = 0;
        *x2 = width - 1;
        return true;
    }

    const int32_t edge_y = y < radius ? radius - y : y - (height - 1 - radius);
    int32_t inset = 0;
    while ((inset + 1) * (inset + 1) + edge_y * edge_y <= radius * radius) {
        ++inset;
    }
    *x1 = radius - inset;
    *x2 = width - radius + inset - 1;
    if (*x1 < 0) {
        *x1 = 0;
    }
    if (*x2 >= width) {
        *x2 = width - 1;
    }
    return *x1 <= *x2;
}
