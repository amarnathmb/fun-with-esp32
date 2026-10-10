#pragma once

#include <Arduino.h>

struct ClockSettings
{
  uint32_t weatherIntervalMs;
  bool serialPrintsEnabled;
  bool use24HourFormat;
  uint32_t weatherLabelColor;
  uint32_t hourLabelColor;
  uint32_t secondLabelColor;
  uint32_t dateLabelColor;
  uint32_t dayLabelColor;
  uint32_t secondProgressColor;
  int weatherIconX;
  int weatherIconY;
  int weatherLabelX;
  int weatherLabelY;
  int hourLabelX;
  int hourLabelY;
  int secondLabelX;
  int secondLabelY;
  int dateLabelX;
  int dateLabelY;
  int dayLabelX;
  int dayLabelY;
  double latitude;
  double longitude;
};

#define DAY_LABEL_COLOR             0xef476f
#define DATE_LABEL_COLOR            0x00f0b5
#define SECOND_LABEL_COLOR          0xFF0000
#define SECOND_PROGRESS_COLOR       0xffb400
#define HOUR_LABEL_COLOR            0xFFFFFF
#define WEATHER_LABEL_COLOR         0x0080f8
#define DEFAULT_WEATHER_ICON_X      0
#define DEFAULT_WEATHER_ICON_Y      -80
#define DEFAULT_WEATHER_LABEL_X     0
#define DEFAULT_WEATHER_LABEL_Y     -45
#define DEFAULT_HOUR_LABEL_X        -17
#define DEFAULT_HOUR_LABEL_Y         0
#define DEFAULT_SECOND_LABEL_X       65
#define DEFAULT_SECOND_LABEL_Y       5
#define DEFAULT_DATE_LABEL_X         0
#define DEFAULT_DATE_LABEL_Y         52
#define DEFAULT_DAY_LABEL_X          0
#define DEFAULT_DAY_LABEL_Y          75
#define DEFAULT_LATITUDE            12.964951
#define DEFAULT_LONGITUDE           77.596598

static const uint32_t DEFAULT_WEATHER_INTERVAL_MS = 10UL * 1000UL;
static const uint32_t MIN_WEATHER_INTERVAL_SECONDS = 10;
static const uint32_t MAX_WEATHER_INTERVAL_SECONDS = 3600;
static const char* WEB_USERNAME = "admin";
static const char* DEFAULT_WEB_PASSWORD = "clock1234";
static const size_t MIN_WEB_PASSWORD_LENGTH = 8;
static const size_t MAX_WEB_PASSWORD_LENGTH = 64;

/* Returns a consistent copy of the active settings. */
ClockSettings getSettings();
/* Replaces the active settings snapshot. */
void setSettings(const ClockSettings& updated);
/* Opens Preferences and initializes settings and the stored web password. */
bool initializeSettings();
/* Reports whether the Preferences namespace was opened successfully. */
bool settingsStorageReady();
/* Reloads settings from Preferences without changing persisted values. */
bool reloadSavedSettings();
/* Persists settings and activates them after successful writes. */
bool saveSettings(const ClockSettings& updated);
/* Returns the active web password. */
String getWebPassword();
/* Persists a replacement web password. */
bool saveWebPassword(const String& password);
