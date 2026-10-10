#pragma once

#include <stddef.h>
#include <lvgl.h>

/* Starts the background task that periodically fetches weather data. */
void startWeatherTask();
/* Returns the latest pending icon and status, clearing the pending icon. */
bool takeWeatherUpdate(const lv_image_dsc_t*& icon, char* status, size_t statusSize);
