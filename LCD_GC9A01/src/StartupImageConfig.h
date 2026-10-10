#pragma once

#include <lvgl.h>

/*
 * Optional startup-image hook:
 * 1. Add your generated LVGL image asset under src/icons/.
 * 2. Uncomment and set CLOCK_STARTUP_IMAGE_SYMBOL to its exported symbol.
 * Keep personal image assets out of public commits.
 */
/* #define CLOCK_STARTUP_IMAGE_SYMBOL my_startup_image */

#ifdef CLOCK_STARTUP_IMAGE_SYMBOL
extern "C" const lv_image_dsc_t CLOCK_STARTUP_IMAGE_SYMBOL;
#endif
