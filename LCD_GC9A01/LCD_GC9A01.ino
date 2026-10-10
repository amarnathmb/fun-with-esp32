#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "../secrets.h"
#include "src/ClockDisplay.h"
#include "src/ClockSettings.h"
#include "src/SerialLog.h"
#include "src/WeatherService.h"
#include "src/WebSettings.h"

/* Initializes settings, display hardware, Wi-Fi, web settings, and time sync. */
void setup()
{
  Serial.begin(115200);
  initializeSettings();
  initializeDisplay();

  /* Connect to Wi-Fi before starting the settings server and NTP sync. */
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  SERIAL_LOG_PRINTLN("Connecting to Wi-Fi...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  SERIAL_LOG_PRINTLN("Wi-Fi connected");
  startSettingsServer();
  SERIAL_LOG_PRINT("Settings page: http://");
  SERIAL_LOG_PRINTLN(WiFi.localIP());

  configTime(
    19800,        /* UTC +5:30 */
    0,
    "pool.ntp.org",
    "time.nist.gov");

  SERIAL_LOG_PRINTLN("Waiting for NTP time; startup screen remains visible.");
}

/* Services the web server, LVGL, weather display, and clock updates. */
void loop()
{
  static uint32_t lastUpdate = 0;
  static bool clockStarted = false;

  handleSettingsServerClient();

  if (!clockStarted)
  {
    if (startClock())
    {
      clockStarted = true;
      startWeatherTask();
    }
    handleLvgl();
    delay(5);
    return;
  }

  updateWeatherDisplay();
  handleLvgl();

  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    update_clock();
  }

  delay(5);
}