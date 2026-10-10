#include "WeatherService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ClockSettings.h"
#include "SerialLog.h"
#include "WeatherAssets.h"

namespace {

portMUX_TYPE weatherIconMux = portMUX_INITIALIZER_UNLOCKED;
const lv_image_dsc_t *pendingWeatherIcon = NULL;
char weather_status[25];

}

/* Extracts a named scalar value from a bounded JSON object range. */
static bool getJsonValue(const String& json, const char* key, String& value,
                         unsigned int jsonStart, unsigned int jsonEnd)
{
    String quotedKey = "\"" + String(key) + "\"";
    int keyPosition = json.indexOf(quotedKey, jsonStart);
    if (keyPosition < 0 || static_cast<unsigned int>(keyPosition) >= jsonEnd) {
        return false;
    }

    int separator = json.indexOf(':', keyPosition + quotedKey.length());
    if (separator < 0 || static_cast<unsigned int>(separator) >= jsonEnd) {
        return false;
    }
    unsigned int valueStart = separator + 1;
    while (valueStart < jsonEnd && isspace(json[valueStart])) {
        ++valueStart;
    }

    if (valueStart >= jsonEnd) {
        return false;
    }

    if (json[valueStart] == '"') {
        int valueEnd = json.indexOf('"', valueStart + 1);
        if (valueEnd < 0 || static_cast<unsigned int>(valueEnd) >= jsonEnd) {
            return false;
        }
        value = json.substring(valueStart + 1, valueEnd);
        return true;
    }

    int valueEnd = json.indexOf(',', valueStart);
    if (valueEnd < 0 || static_cast<unsigned int>(valueEnd) >= jsonEnd) {
        valueEnd = json.indexOf('}', valueStart);
    }
    if (valueEnd < 0 || static_cast<unsigned int>(valueEnd) >= jsonEnd) {
        return false;
    }

    value = json.substring(valueStart, valueEnd);
    value.trim();
    return true;
}

/* Returns the readable label for an Open-Meteo weather code. */
static const char* weatherDescription(int code)
{
    if (code == 0) return "Clear sky";
    if (code == 1) return "Mainly clear";
    if (code == 2) return "Partly cloudy";
    if (code == 3) return "Cloudy";
    if (code == 45 || code == 48) return "Fog";
    if (code == 51 || code == 53 || code == 55) return "Drizzle";
    if (code == 56 || code == 57) return "Freezing drizzle";
    if (code == 61 || code == 63 || code == 65) return "Rain";
    if (code == 66 || code == 67) return "Freezing rain";
    if (code == 71 || code == 73 || code == 75 || code == 77) return "Snow";
    if (code == 80 || code == 81 || code == 82) return "Rain showers";
    if (code == 85 || code == 86) return "Snow showers";
    if (code == 95 || code == 96 || code == 99) return "Thunderstorm";
    return "Unknown";
}

/* Maps an Open-Meteo weather code to its LVGL icon asset. */
static const lv_image_dsc_t* weatherIconForCode(int code)
{
    switch (code) {
        case 0: return &sunny;
        case 1: return &cloudy_s_sunny;
        case 2: return &partly_cloudy;
        case 3: return &cloudy;
        case 45:
        case 48: return &fog;
        case 51:
        case 53: return &cloudy_s_rain;
        case 55:
        case 61: return &rain_light;
        case 63: return &rain;
        case 65:
        case 82:
        case 95:
        case 96:
        case 99: return &rain_heavy;
        case 56:
        case 57: return &cloudy_s_snow;
        case 66: return &rain_s_snow;
        case 67: return &snow_s_rain;
        case 71: return &snow_light;
        case 73: return &snow;
        case 75: return &snow_heavy;
        case 77: return &snow_s_cloudy;
        case 80: return &rain_s_sunny;
        case 81: return &rain_s_cloudy;
        case 85:
        case 86: return &snow_s_cloudy;
        default: return NULL;
    }
}

/* Fetches weather data, queues the display update, and logs a concise summary. */
static void printWeather()
{
    ClockSettings currentSettings = getSettings();
    if (WiFi.status() != WL_CONNECTED) {
        SERIAL_LOG_PRINTLN("Wi-Fi disconnected; reconnecting...");
        WiFi.reconnect();
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); /* Uses TLS without certificate verification. */

    String requestUrl;
    requestUrl.reserve(300);
    requestUrl = "https://api.open-meteo.com/v1/forecast?latitude=";
    requestUrl += String(currentSettings.latitude, 6);
    requestUrl += "&longitude=";
    requestUrl += String(currentSettings.longitude, 6);
    requestUrl += "&current=temperature_2m,weather_code";
    requestUrl += "&timezone=auto";

    HTTPClient http;
    if (!http.begin(client, requestUrl)) {
        SERIAL_LOG_PRINTLN("Could not start the weather request.");
        return;
    }

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        SERIAL_LOG_PRINTF("Weather request failed (HTTP %d): %s\n",
                          status, http.errorToString(status).c_str());
        http.end();
        return;
    }

    String response = http.getString();
    http.end();

    int currentStart = response.indexOf("\"current\":{");
    int currentEnd = currentStart < 0 ? -1 : response.indexOf('}', currentStart);
    if (currentEnd < 0) {
        SERIAL_LOG_PRINTLN("Weather response did not contain current conditions.");
        return;
    }
    String temperature, code;
    unsigned int currentEndExclusive = currentEnd + 1;
    if (!getJsonValue(response, "temperature_2m", temperature,
                      currentStart, currentEndExclusive) ||
        !getJsonValue(response, "weather_code", code,
                      currentStart, currentEndExclusive)) {
        SERIAL_LOG_PRINTLN("Weather response was missing one or more expected fields.");
        return;
    }

    int weatherCode = code.toInt();
    const lv_image_dsc_t *icon = weatherIconForCode(weatherCode);
    if (icon != NULL) {
        portENTER_CRITICAL(&weatherIconMux);
        pendingWeatherIcon = icon;
        snprintf(weather_status, sizeof(weather_status), "%s°C", temperature.c_str());
        portEXIT_CRITICAL(&weatherIconMux);
    }

    SERIAL_LOG_PRINTF("Weather: %s, %s°C\r\n",
                      weatherDescription(weatherCode), temperature.c_str());
}

/* Runs periodic weather requests on a dedicated FreeRTOS task. */
static void weatherTask(void *parameter)
{
    (void)parameter;

    for (;;) {
        printWeather();
        uint32_t waitStarted = millis();
        while (millis() - waitStarted < getSettings().weatherIntervalMs) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}


/* Creates the background weather task. */
void startWeatherTask()
{
  BaseType_t taskCreated = xTaskCreatePinnedToCore(
    weatherTask,
    "WeatherTask",
    10000,
    NULL,
    1,
    NULL,
    0);

  if (taskCreated != pdPASS)
  {
    SERIAL_LOG_PRINTLN("Failed to create the weather task.");
  }
}

/* Transfers the pending weather icon and status to the caller. */
bool takeWeatherUpdate(const lv_image_dsc_t*& icon, char* status, size_t statusSize)
{
  portENTER_CRITICAL(&weatherIconMux);
  icon = pendingWeatherIcon;
  pendingWeatherIcon = NULL;
  memcpy(status, weather_status, statusSize);
  portEXIT_CRITICAL(&weatherIconMux);
  return icon != NULL;
}