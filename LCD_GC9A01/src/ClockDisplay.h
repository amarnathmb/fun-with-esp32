#pragma once

/* Initializes the GC9A01 display and LVGL driver. */
void initializeDisplay();
/* Starts the clock UI when synchronized local time becomes available. */
bool startClock();
/* Updates clock labels and the seconds progress ring. */
void update_clock();
/* Applies persisted text and seconds-ring colors to the display. */
void applyConfiguredColors();
/* Applies persisted positions to configurable display objects. */
void applyConfiguredLayout();
/* Applies the latest pending weather update to the display. */
void updateWeatherDisplay();
/* Services LVGL timers and rendering. */
void handleLvgl();
