#include "ClockDisplay.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <math.h>
#include <time.h>

#include "ClockSettings.h"
#include "SerialLog.h"
#include "StartupImageConfig.h"
#include "WeatherAssets.h"
#include "WeatherService.h"

#define TFT_CS                      5
#define TFT_DC                      16
#define TFT_RST                     414

#define SCREEN_WIDTH                240
#define SCREEN_HEIGHT               240

#define SECOND_DOT_COUNT            60
#define SECOND_DOT_SIZE             5
#define SECOND_DOT_RADIUS           105
#define DRAW_BUF_LINES              10

#define COLOR_BG                    0x000000
#define SECOND_PROGRESS_BG_COLOR    0x202020

#define WEATHER_FONT                &lv_font_montserrat_20
#define HOUR_FONT                   &lv_font_ubuntu_48
#define SECOND_FONT                 &lv_font_ubuntu_34
#define DATE_FONT                   &lv_font_ubuntu_20
#define DAY_FONT                    &lv_font_ubuntu_20

static Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
static uint16_t draw_buf[SCREEN_WIDTH * DRAW_BUF_LINES];
static lv_display_t *display;
#if defined(CLOCK_STARTUP_IMAGE_SYMBOL)
static lv_obj_t *startup_image;
#endif
static bool clockStarted = false;

static lv_obj_t *hour_label;
static lv_obj_t *weather_label;
static lv_obj_t *second_label;
static lv_obj_t *date_label;
static lv_obj_t *day_label;
static lv_obj_t *weather_icon;
static lv_obj_t *second_dots[SECOND_DOT_COUNT];
static int lastClockSecond = 0;

static uint32_t millis_cb();
static void create_ui();

/* Transfers an LVGL render area to the GC9A01 display. */
static void flush_cb(
  lv_display_t *disp,
  const lv_area_t *area,
  uint8_t *px_map)
{
  uint32_t width =
    area->x2 - area->x1 + 1;

  uint32_t height =
    area->y2 - area->y1 + 1;

  uint16_t *pixels =
    (uint16_t *)px_map;

  tft.startWrite();

  tft.setAddrWindow(
    area->x1,
    area->y1,
    width,
    height);

  tft.writePixels(
    pixels,
    width * height,
    true,
    false);

  tft.endWrite();

  lv_display_flush_ready(disp);
}

/* Supplies LVGL with the Arduino millisecond tick. */
static uint32_t millis_cb()
{
  return millis();
}

/* Creates clock objects and applies the configured layout. */
static void create_ui()
{
  lv_obj_t *screen = lv_screen_active();

  /* Set the screen background and create the weather elements. */
  lv_obj_set_style_bg_color(
    screen,
    lv_color_hex(COLOR_BG),
    0);

  weather_icon = lv_image_create(screen);
  lv_obj_set_style_bg_opa(weather_icon, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);

  weather_label =
    lv_label_create(screen);

  lv_label_set_text(
    weather_label,
    "00");

  lv_obj_set_style_text_color(
    weather_label,
      lv_color_hex(getSettings().weatherLabelColor),
    0);

  lv_obj_set_style_text_font(
    weather_label,
    WEATHER_FONT,
    0);

  /* Create the seconds ring, starting at 12 o'clock. */
  for (int i = 0; i < SECOND_DOT_COUNT; i++)
  {
    float angle = (2.0f * PI * i) / SECOND_DOT_COUNT;
    int x = (SCREEN_WIDTH - SECOND_DOT_SIZE) / 2
      + (int)roundf(sinf(angle) * SECOND_DOT_RADIUS);
    int y = (SCREEN_HEIGHT - SECOND_DOT_SIZE) / 2
      - (int)roundf(cosf(angle) * SECOND_DOT_RADIUS);

    second_dots[i] = lv_obj_create(screen);
    lv_obj_set_size(second_dots[i], SECOND_DOT_SIZE, SECOND_DOT_SIZE);
    lv_obj_set_pos(second_dots[i], x, y);
    lv_obj_set_style_radius(second_dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(second_dots[i], 0, 0);
    lv_obj_set_style_bg_opa(second_dots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
      second_dots[i],
      lv_color_hex(COLOR_BG),
      0);
  }

  /* Create the main time and seconds labels. */
  hour_label =
    lv_label_create(screen);

  lv_label_set_text(
    hour_label,
    "00");

  lv_obj_set_style_text_color(
    hour_label,
    lv_color_hex(getSettings().hourLabelColor),
    0);

  lv_obj_set_style_text_font(
    hour_label,
    HOUR_FONT,
    0);

  /* Place the seconds label beside the main time. */
  second_label =
    lv_label_create(screen);

  lv_label_set_text(
    second_label,
    "00");

  lv_obj_set_style_text_color(
    second_label,
    lv_color_hex(getSettings().secondLabelColor),
    0);

  lv_obj_set_style_text_font(
    second_label,
    SECOND_FONT,
    0);

  /* Create date and weekday labels. */
  date_label =
    lv_label_create(screen);

  lv_label_set_text(
    date_label,
    "---");

  lv_obj_set_style_text_color(
    date_label,
    lv_color_hex(getSettings().dateLabelColor),
    0);

  lv_obj_set_style_text_font(
    date_label,
    DATE_FONT,
    0);

  day_label =
    lv_label_create(screen);

  lv_label_set_text(
    day_label,
    "---");

  lv_obj_set_style_text_color(
    day_label,
    lv_color_hex(getSettings().dayLabelColor),
    0);

  lv_obj_set_style_text_font(
    day_label,
    DAY_FONT,
    0);

  applyConfiguredLayout();
}

/* Applies saved offsets to all configurable LVGL objects. */
void applyConfiguredLayout()
{
  if (weather_icon == NULL || weather_label == NULL ||
      hour_label == NULL || second_label == NULL ||
      date_label == NULL || day_label == NULL) {
    return;
  }

  ClockSettings current = getSettings();
  lv_obj_align(weather_icon, LV_ALIGN_CENTER, current.weatherIconX, current.weatherIconY);
  lv_obj_align(weather_label, LV_ALIGN_CENTER, current.weatherLabelX, current.weatherLabelY);
  lv_obj_align(hour_label, LV_ALIGN_CENTER, current.hourLabelX, current.hourLabelY);
  lv_obj_align(second_label, LV_ALIGN_CENTER, current.secondLabelX, current.secondLabelY);
  lv_obj_align(date_label, LV_ALIGN_CENTER, current.dateLabelX, current.dateLabelY);
  lv_obj_align(day_label, LV_ALIGN_CENTER, current.dayLabelX, current.dayLabelY);
}

/* Applies saved text and seconds-ring colors to the clock objects. */
void applyConfiguredColors()
{
  if (!clockStarted) {
    return;
  }

  ClockSettings current = getSettings();
  lv_obj_set_style_text_color(weather_label,
                              lv_color_hex(current.weatherLabelColor), 0);
  lv_obj_set_style_text_color(hour_label,
                              lv_color_hex(current.hourLabelColor), 0);
  lv_obj_set_style_text_color(second_label,
                              lv_color_hex(current.secondLabelColor), 0);
  lv_obj_set_style_text_color(date_label,
                              lv_color_hex(current.dateLabelColor), 0);
  lv_obj_set_style_text_color(day_label,
                              lv_color_hex(current.dayLabelColor), 0);

  for (int i = 0; i < SECOND_DOT_COUNT; ++i) {
    lv_obj_set_style_bg_color(
      second_dots[i],
      lv_color_hex(i <= lastClockSecond
        ? current.secondProgressColor
        : COLOR_BG),
      0);
  }
}

/* Updates time, date, weekday, and the seconds ring from local time. */
void update_clock()
{
  struct tm timeinfo;
  ClockSettings currentSettings = getSettings();

  if (!getLocalTime(&timeinfo, 100))
  {
    SERIAL_LOG_PRINTLN("Failed to get time");
    return;
  }
  lastClockSecond = timeinfo.tm_sec;

  /* Update the main time and seconds labels. */
  char timeString[6];

  strftime(
    timeString,
    sizeof(timeString),
    currentSettings.use24HourFormat ? "%H:%M" : "%I:%M",
    &timeinfo);

  lv_label_set_text(
    hour_label,
    timeString);

  strftime(
    timeString,
    sizeof(timeString),
    "%S",
    &timeinfo);

  lv_label_set_text(
    second_label,
    timeString);

  /* Update the date and weekday labels. */
  char dateString[12];
  char dayString[20];

  strftime(
    dateString,
    sizeof(dateString),
    "%d %b %Y",
    &timeinfo);

  strftime(
    dayString,
    sizeof(dayString),
    "%A",
    &timeinfo);

  lv_label_set_text(
    date_label,
    dateString);

  lv_label_set_text(
    day_label,
    dayString);

  /* Update the seconds ring progressively. */
  for (int i = 0; i < SECOND_DOT_COUNT; i++)
  {
    lv_obj_set_style_bg_color(
      second_dots[i],
      lv_color_hex(i <= timeinfo.tm_sec
        ? currentSettings.secondProgressColor
        : COLOR_BG),
      0);
  }

}

/* Initializes the GC9A01, LVGL display driver, and optional startup image. */
void initializeDisplay()
{
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  lv_init();
  lv_tick_set_cb(millis_cb);

  display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_buffers(
    display,
    draw_buf,
    NULL,
    sizeof(draw_buf),
    LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush_cb);

#if defined(CLOCK_STARTUP_IMAGE_SYMBOL)
  startup_image = lv_image_create(lv_screen_active());
  lv_image_set_src(startup_image, &CLOCK_STARTUP_IMAGE_SYMBOL);
  lv_obj_center(startup_image);
#endif

  lv_timer_handler();
  delay(100);
  SERIAL_LOG_PRINTLN("Display initialized");
}

/* Starts the clock UI once local time is available. */
bool startClock()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0))
  {
    return false;
  }

  SERIAL_LOG_PRINTF(
    "Time synchronized: %02d:%02d:%02d\n",
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec);

#if defined(CLOCK_STARTUP_IMAGE_SYMBOL)
  lv_obj_del(startup_image);
  startup_image = NULL;
#endif
  create_ui();
  update_clock();
  clockStarted = true;
  return true;
}

/* Applies the latest weather service update to the LVGL objects. */
void updateWeatherDisplay()
{
  const lv_image_dsc_t *icon = NULL;
  char currentWeatherStatus[25];
  takeWeatherUpdate(icon, currentWeatherStatus, sizeof(currentWeatherStatus));

  lv_label_set_text(weather_label, currentWeatherStatus);
  if (icon != NULL)
  {
    lv_image_set_src(weather_icon, icon);
    lv_obj_clear_flag(weather_icon, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Runs LVGL's timer and rendering handler. */
void handleLvgl()
{
  lv_timer_handler();
}