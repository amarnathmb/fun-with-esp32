#include "ClockSettings.h"

#include <math.h>
#include <Preferences.h>

#include "SerialLog.h"

namespace {

portMUX_TYPE settingsMux = portMUX_INITIALIZER_UNLOCKED;
/* Builds a settings object populated with the firmware's default values. */
ClockSettings makeDefaultSettings()
{
  return {
    DEFAULT_WEATHER_INTERVAL_MS,
    true,
    false,
    WEATHER_LABEL_COLOR,
    HOUR_LABEL_COLOR,
    SECOND_LABEL_COLOR,
    DATE_LABEL_COLOR,
    DAY_LABEL_COLOR,
    SECOND_PROGRESS_COLOR,
    DEFAULT_WEATHER_ICON_X,
    DEFAULT_WEATHER_ICON_Y,
    DEFAULT_WEATHER_LABEL_X,
    DEFAULT_WEATHER_LABEL_Y,
    DEFAULT_HOUR_LABEL_X,
    DEFAULT_HOUR_LABEL_Y,
    DEFAULT_SECOND_LABEL_X,
    DEFAULT_SECOND_LABEL_Y,
    DEFAULT_DATE_LABEL_X,
    DEFAULT_DATE_LABEL_Y,
    DEFAULT_DAY_LABEL_X,
    DEFAULT_DAY_LABEL_Y,
    DEFAULT_LATITUDE,
    DEFAULT_LONGITUDE
  };
}

ClockSettings settings = makeDefaultSettings();

Preferences preferences;
bool preferencesReady = false;
String webPassword = DEFAULT_WEB_PASSWORD;

/* Loads persisted settings, validates bounded values, and updates shared state. */
void loadSavedSettingsFromPreferences()
{
  uint32_t savedIntervalSeconds =
    preferences.getUInt("interval", DEFAULT_WEATHER_INTERVAL_MS / 1000);
  if (savedIntervalSeconds < MIN_WEATHER_INTERVAL_SECONDS ||
      savedIntervalSeconds > MAX_WEATHER_INTERVAL_SECONDS) {
    savedIntervalSeconds = DEFAULT_WEATHER_INTERVAL_MS / 1000;
  }

  ClockSettings savedSettings = {
    savedIntervalSeconds * 1000UL,
    preferences.getBool("serial", true),
    preferences.getBool("24hour", false),
    preferences.getUInt("wcolor", WEATHER_LABEL_COLOR) & 0xFFFFFF,
    preferences.getUInt("hcolor", HOUR_LABEL_COLOR) & 0xFFFFFF,
    preferences.getUInt("scolor", SECOND_LABEL_COLOR) & 0xFFFFFF,
    preferences.getUInt("dcolor", DATE_LABEL_COLOR) & 0xFFFFFF,
    preferences.getUInt("daycolor", DAY_LABEL_COLOR) & 0xFFFFFF,
    preferences.getUInt("pcolor", SECOND_PROGRESS_COLOR) & 0xFFFFFF,
    preferences.getInt("wix", DEFAULT_WEATHER_ICON_X),
    preferences.getInt("wiy", DEFAULT_WEATHER_ICON_Y),
    preferences.getInt("wlx", DEFAULT_WEATHER_LABEL_X),
    preferences.getInt("wly", DEFAULT_WEATHER_LABEL_Y),
    preferences.getInt("hlx", DEFAULT_HOUR_LABEL_X),
    preferences.getInt("hly", DEFAULT_HOUR_LABEL_Y),
    preferences.getInt("slx", DEFAULT_SECOND_LABEL_X),
    preferences.getInt("sly", DEFAULT_SECOND_LABEL_Y),
    preferences.getInt("dlx", DEFAULT_DATE_LABEL_X),
    preferences.getInt("dly", DEFAULT_DATE_LABEL_Y),
    preferences.getInt("daylx", DEFAULT_DAY_LABEL_X),
    preferences.getInt("dayly", DEFAULT_DAY_LABEL_Y),
    preferences.getDouble("latitude", DEFAULT_LATITUDE),
    preferences.getDouble("longitude", DEFAULT_LONGITUDE)
  };
  if (!isfinite(savedSettings.latitude) ||
      savedSettings.latitude < -90.0 ||
      savedSettings.latitude > 90.0) {
    savedSettings.latitude = DEFAULT_LATITUDE;
  }
  if (!isfinite(savedSettings.longitude) ||
      savedSettings.longitude < -180.0 ||
      savedSettings.longitude > 180.0) {
    savedSettings.longitude = DEFAULT_LONGITUDE;
  }
  setSettings(savedSettings);
}

}

/* Returns a consistent copy of the active settings. */
ClockSettings getSettings()
{
  ClockSettings current;
  portENTER_CRITICAL(&settingsMux);
  current = settings;
  portEXIT_CRITICAL(&settingsMux);
  return current;
}

/* Replaces the active settings snapshot. */
void setSettings(const ClockSettings& updated)
{
  portENTER_CRITICAL(&settingsMux);
  settings = updated;
  portEXIT_CRITICAL(&settingsMux);
}

/* Opens Preferences and initializes settings and the stored web password. */
bool initializeSettings()
{
  preferencesReady = preferences.begin("clockcfg", false);
  if (!preferencesReady) {
    SERIAL_LOG_PRINTLN("Could not open settings storage; using defaults.");
    return false;
  }

  loadSavedSettingsFromPreferences();

  String savedPassword = preferences.getString("webpass", DEFAULT_WEB_PASSWORD);
  if (savedPassword.length() >= MIN_WEB_PASSWORD_LENGTH &&
      savedPassword.length() <= MAX_WEB_PASSWORD_LENGTH) {
    webPassword = savedPassword;
  } else {
    webPassword = DEFAULT_WEB_PASSWORD;
    SERIAL_LOG_PRINTLN("Stored web password invalid; using default password.");
  }

  return true;
}

/* Reloads settings from Preferences without changing persisted values. */
bool reloadSavedSettings()
{
  if (!preferencesReady) {
    return false;
  }

  loadSavedSettingsFromPreferences();
  return true;
}

/* Reports whether the Preferences namespace was opened successfully. */
bool settingsStorageReady()
{
  return preferencesReady;
}

/* Persists settings and updates the active snapshot only after all writes pass. */
bool saveSettings(const ClockSettings& updated)
{
  if (!preferencesReady ||
      preferences.putUInt("interval", updated.weatherIntervalMs / 1000) != sizeof(uint32_t) ||
      preferences.putBool("serial", updated.serialPrintsEnabled) != sizeof(bool) ||
      preferences.putBool("24hour", updated.use24HourFormat) != sizeof(bool) ||
      preferences.putUInt("wcolor", updated.weatherLabelColor) != sizeof(uint32_t) ||
      preferences.putUInt("hcolor", updated.hourLabelColor) != sizeof(uint32_t) ||
      preferences.putUInt("scolor", updated.secondLabelColor) != sizeof(uint32_t) ||
      preferences.putUInt("dcolor", updated.dateLabelColor) != sizeof(uint32_t) ||
      preferences.putUInt("daycolor", updated.dayLabelColor) != sizeof(uint32_t) ||
      preferences.putUInt("pcolor", updated.secondProgressColor) != sizeof(uint32_t) ||
      preferences.putInt("wix", updated.weatherIconX) != sizeof(int32_t) ||
      preferences.putInt("wiy", updated.weatherIconY) != sizeof(int32_t) ||
      preferences.putInt("wlx", updated.weatherLabelX) != sizeof(int32_t) ||
      preferences.putInt("wly", updated.weatherLabelY) != sizeof(int32_t) ||
      preferences.putInt("hlx", updated.hourLabelX) != sizeof(int32_t) ||
      preferences.putInt("hly", updated.hourLabelY) != sizeof(int32_t) ||
      preferences.putInt("slx", updated.secondLabelX) != sizeof(int32_t) ||
      preferences.putInt("sly", updated.secondLabelY) != sizeof(int32_t) ||
      preferences.putInt("dlx", updated.dateLabelX) != sizeof(int32_t) ||
      preferences.putInt("dly", updated.dateLabelY) != sizeof(int32_t) ||
      preferences.putInt("daylx", updated.dayLabelX) != sizeof(int32_t) ||
      preferences.putInt("dayly", updated.dayLabelY) != sizeof(int32_t) ||
      preferences.putDouble("latitude", updated.latitude) != sizeof(double) ||
      preferences.putDouble("longitude", updated.longitude) != sizeof(double)) {
    return false;
  }

  setSettings(updated);
  return true;
}

/* Returns the active web password. */
String getWebPassword()
{
  return webPassword;
}

/* Persists a new web password and updates the active password on success. */
bool saveWebPassword(const String& password)
{
  if (!preferencesReady ||
      preferences.putString("webpass", password) != password.length()) {
    return false;
  }

  webPassword = password;
  return true;
}
