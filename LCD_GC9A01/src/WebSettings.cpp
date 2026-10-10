#include "WebSettings.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <math.h>
#include <stdlib.h>
#include <WebServer.h>

#include "ClockDisplay.h"
#include "ClockSettings.h"
#include "SerialLog.h"

namespace {

WebServer server(80);

/* Authenticates the current request or challenges the browser for credentials. */
bool authenticateWebRequest()
{
  String password = getWebPassword();
  if (server.authenticate(WEB_USERNAME, password.c_str())) {
    return true;
  }

  server.requestAuthentication(BASIC_AUTH, "ESP32 Clock Settings");
  return false;
}

}

/* Formats a 24-bit color as an HTML hexadecimal color value. */
static String colorInputValue(uint32_t color)
{
  char value[8];
  snprintf(value, sizeof(value), "#%06lX",
           static_cast<unsigned long>(color & 0xFFFFFF));
  return String(value);
}

/* Appends a color picker to the generated settings form. */
static void appendColorPicker(String& page, const char* id, const char* label,
                       uint32_t color)
{
  page += F("<div class=\"field\"><label class=\"field-label\" for=\"");
  page += id;
  page += F("\">");
  page += label;
  page += F("</label><input type=\"color\" id=\"");
  page += id;
  page += F("\" name=\"");
  page += id;
  page += F("\" value=\"");
  page += colorInputValue(color);
  page += F("\" required></div>");
}

/* Parses a #RRGGBB color value. */
static bool parseColorInput(const String& input, uint32_t& color)
{
  if (input.length() != 7 || input[0] != '#') {
    return false;
  }

  color = 0;
  for (size_t i = 1; i < input.length(); ++i) {
    char digit = input[i];
    uint8_t value;
    if (digit >= '0' && digit <= '9') {
      value = digit - '0';
    } else if (digit >= 'a' && digit <= 'f') {
      value = digit - 'a' + 10;
    } else if (digit >= 'A' && digit <= 'F') {
      value = digit - 'A' + 10;
    } else {
      return false;
    }
    color = (color << 4) | value;
  }
  return true;
}

/* Parses and validates a floating-point coordinate. */
static bool parseCoordinate(const String& input, double minimum, double maximum,
                     double& coordinate)
{
  if (input.isEmpty()) {
    return false;
  }

  char* end = NULL;
  coordinate = strtod(input.c_str(), &end);
  return end != input.c_str() &&
         *end == '\0' &&
         isfinite(coordinate) &&
         coordinate >= minimum &&
         coordinate <= maximum;
}

/* Parses and validates a bounded integer input. */
static bool parseIntegerInput(const String& input, int& value,
                              int minimum, int maximum)
{
  if (input.isEmpty()) {
    return false;
  }

  char* end = NULL;
  long parsed = strtol(input.c_str(), &end, 10);
  if (end == input.c_str() || *end != '\0') {
    return false;
  }
  value = static_cast<int>(parsed);
  return value >= minimum && value <= maximum;
}

/* Appends horizontal and vertical position sliders to the settings form. */
static void appendPositionControl(String& page, const char* prefix,
                                  const char* label, int x, int y)
{
  page += F("<div class=\"field\"><label class=\"field-label\">");
  page += label;
  page += F("</label><div style=\"display:grid;grid-template-columns:1fr;gap:12px;\">");

  page += F("<div class=\"range-field\"><span title=\"Move left\" aria-label=\"Move left\">←</span><input type=\"range\" id=\"");
  page += prefix;
  page += F("X\" name=\"");
  page += prefix;
  page += F("X\" min=\"-120\" max=\"120\" step=\"1\" value=\"");
  page += String(x);
  page += F("\" oninput=\"this.nextElementSibling.textContent=this.value\"><output>");
  page += String(x);
  page += F("</output><span title=\"Move right\" aria-label=\"Move right\">→</span></div>");

  page += F("<div class=\"range-field\"><span title=\"Move up\" aria-label=\"Move up\">↑</span><input type=\"range\" id=\"");
  page += prefix;
  page += F("Y\" name=\"");
  page += prefix;
  page += F("Y\" min=\"-120\" max=\"120\" step=\"1\" value=\"");
  page += String(y);
  page += F("\" oninput=\"this.nextElementSibling.textContent=this.value\"><output>");
  page += String(y);
  page += F("</output><span title=\"Move down\" aria-label=\"Move down\">↓</span></div>");

  page += F("</div></div>");
}

/* Builds the authenticated device settings page. */
static String settingsPage()
{
  ClockSettings current = getSettings();
  String page;
  page.reserve(3600);

  page += F(R"HTML(
  <!doctype html>
  <html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="dark">
    <title>Clock Settings</title>

    <style>
      :root {
        color-scheme: dark;
        --bg: #0b0f14;
        --card: #131a23;
        --border: #283342;
        --text: #edf2f7;
        --muted: #98a6b8;
        --accent: #39ffb6;
        --accent-text: #07130f;
        --input: #0d131b;
        --radius: 16px;
      }

      * {
        box-sizing: border-box;
      }

      body {
        margin: 0;
        padding: 32px 16px;
        background: var(--bg);
        color: var(--text);
        font: 15px/1.5 system-ui, -apple-system, BlinkMacSystemFont,
              "Segoe UI", sans-serif;
        -webkit-font-smoothing: antialiased;
      }

      main {
        max-width: 480px;
        margin: 0 auto;
      }

      header {
        margin-bottom: 28px;
      }

      .eyebrow {
        color: var(--accent);
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 2px;
        text-transform: uppercase;
      }

      h1 {
        margin: 8px 0 6px;
        font-size: 30px;
        line-height: 1.2;
        letter-spacing: -0.8px;
      }

      .subtitle {
        margin: 0;
        color: var(--muted);
        font-size: 14px;
      }

      section {
        margin-bottom: 18px;
        padding: 22px;
        background: var(--card);
        border: 1px solid var(--border);
        border-radius: var(--radius);
      }

      h2 {
        margin: 0 0 20px;
        font-size: 17px;
        font-weight: 650;
        letter-spacing: -0.2px;
      }

      details > summary {
        display: flex;
        align-items: center;
        justify-content: space-between;
        cursor: pointer;
        list-style: none;
      }

      details > summary::-webkit-details-marker {
        display: none;
      }

      details > summary::after {
        content: "+";
        color: var(--accent);
        font-size: 22px;
        line-height: 1;
      }

      details[open] > summary {
        margin-bottom: 20px;
      }

      details[open] > summary::after {
        content: "−";
      }

      details > summary h2 {
        margin: 0;
      }

      .field {
        margin-bottom: 22px;
      }

      .field-label {
        display: block;
        margin-bottom: 8px;
        font-weight: 550;
      }

      .hint {
        display: block;
        margin-top: 6px;
        color: var(--muted);
        font-size: 12px;
      }

      input[type="number"],
      input[type="password"] {
        display: block;
        width: 100%;
        min-height: 46px;
        padding: 11px 13px;
        border: 1px solid var(--border);
        border-radius: 10px;
        outline: none;
        background: var(--input);
        color: var(--text);
        font: inherit;
        transition: border-color 150ms, box-shadow 150ms;
      }

      input[type="range"] {
        width: 100%;
        accent-color: var(--accent);
        cursor: pointer;
      }

      .range-field {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
      }

      .range-field span {
        width: 12px;
        flex: 0 0 12px;
        color: var(--muted);
        font-weight: 600;
        text-align: center;
      }

      .range-field input[type="range"] {
        flex: 1 1 auto;
        min-width: 0;
      }

      .range-field output {
        min-width: 40px;
        text-align: right;
        color: var(--text);
        font-weight: 600;
      }

      input:focus-visible,
      button:focus-visible {
        outline: 2px solid var(--accent);
        outline-offset: 3px;
      }

      input[type="number"]:focus,
      input[type="password"]:focus {
        border-color: var(--accent);
        box-shadow: 0 0 0 3px #39ffb61a;
      }

      input[type="color"] {
        display: block;
        width: 100%;
        height: 46px;
        padding: 5px;
        border: 1px solid var(--border);
        border-radius: 10px;
        background: var(--input);
        cursor: pointer;
      }

      fieldset {
        min-width: 0;
        margin: 0 0 22px;
        padding: 0;
        border: 0;
      }

      legend {
        margin-bottom: 10px;
        font-weight: 550;
      }

      .radio-options {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 10px;
      }

      .tabs {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        margin-bottom: 18px;
      }

      .tab-button {
        width: auto;
        flex: 1 1 0;
        min-height: 40px;
        padding: 10px 14px;
        border: 1px solid var(--border);
        border-radius: 10px;
        background: var(--input);
        color: var(--text);
        font: inherit;
        font-weight: 600;
        cursor: pointer;
      }

      .tab-button.active {
        background: var(--accent);
        color: var(--accent-text);
        border-color: var(--accent);
      }

      .tab-panel {
        display: none;
      }

      .tab-panel.active {
        display: block;
      }

      .choice {
        display: flex;
        min-height: 46px;
        align-items: center;
        gap: 9px;
        padding: 10px 12px;
        border: 1px solid var(--border);
        border-radius: 10px;
        background: var(--input);
        cursor: pointer;
        transition: border-color 150ms, background 150ms;
      }

      .choice:has(input:checked) {
        border-color: var(--accent);
        background: #39ffb60d;
      }

      input[type="radio"],
      input[type="checkbox"] {
        width: 17px;
        height: 17px;
        margin: 0;
        flex-shrink: 0;
        accent-color: var(--accent);
        cursor: pointer;
      }

      .toggle-row {
        display: flex;
        align-items: center;
        gap: 11px;
        cursor: pointer;
      }

      .toggle-text {
        font-weight: 500;
      }

      .toggle-description {
        display: block;
        margin-top: 2px;
        color: var(--muted);
        font-size: 12px;
        font-weight: 400;
      }

      button {
        width: 100%;
        min-height: 46px;
        padding: 12px 16px;
        border: 0;
        border-radius: 10px;
        background: var(--accent);
        color: var(--accent-text);
        font: inherit;
        font-weight: 700;
        cursor: pointer;
        transition: filter 150ms, transform 150ms;
      }

      button:hover {
        filter: brightness(0.92);
      }

      button:active {
        transform: scale(0.99);
      }

      .secondary-button {
        background: #222d3b;
        color: var(--text);
        border: 1px solid #354254;
      }

      .divider {
        height: 1px;
        margin: 22px 0;
        background: var(--border);
      }

      footer {
        padding: 8px 0;
        color: var(--muted);
        font-size: 12px;
        text-align: center;
      }

      footer a {
        color: var(--accent);
        text-decoration: none;
        font-weight: 600;
      }

      footer a:hover {
        text-decoration: underline;
      }

      @media (max-width: 360px) {
        body {
          padding: 24px 12px;
        }

        section {
          padding: 17px;
        }

        h1 {
          font-size: 26px;
        }
      }

      @media (prefers-reduced-motion: reduce) {
        *,
        *::before,
        *::after {
          transition: none !important;
        }
      }
    </style>
  </head>

  <body>
    <main>
      <header>
        <div class="eyebrow">ESP32 · CONTROL PANEL</div>
        <h1>Clock settings</h1>
        <p class="subtitle">
          Personalize your clock and manage device preferences.
        </p>
      </header>

      <section>
        <h2>Display &amp; updates</h2>

        <form method="post" action="/settings">
          <div class="tabs" role="tablist" aria-label="Settings tabs">
            <button type="button" class="tab-button active" data-tab="general" role="tab" aria-selected="true">General</button>
            <button type="button" class="tab-button" data-tab="colors" role="tab" aria-selected="false">Colors</button>
            <button type="button" class="tab-button" data-tab="position" role="tab" aria-selected="false">Position</button>
          </div>

          <div class="tab-panel active" id="general" role="tabpanel">
            <div class="field">
              <label class="field-label" for="interval">
                Weather update interval
              </label>
              <input
                type="number"
                id="interval"
                name="interval"
                min=")HTML");

  page += String(MIN_WEATHER_INTERVAL_SECONDS);

  page += F(R"HTML("
                max=")HTML");

  page += String(MAX_WEATHER_INTERVAL_SECONDS);

  page += F(R"HTML("
                value=")HTML");

  page += String(current.weatherIntervalMs / 1000);

  page += F(R"HTML("
                required
              >
              <span class="hint">
                Refresh weather data periodically, in seconds.
              </span>
            </div>

            <div class="field">
              <label class="field-label" for="latitude">Latitude</label>
              <input
                type="number"
                id="latitude"
                name="latitude"
                min="-90"
                max="90"
                step="any"
                value=")HTML");

  page += String(current.latitude, 6);

  page += F(R"HTML("
                required
              >
            </div>
            <div class="field">
              <label class="field-label" for="longitude">Longitude</label>
              <input
                type="number"
                id="longitude"
                name="longitude"
                min="-180"
                max="180"
                step="any"
                value=")HTML");

  page += String(current.longitude, 6);

  page += F(R"HTML("
                required
              >
              <span class="hint">Coordinates in decimal degrees.</span>
            </div>

            <fieldset>
              <legend>Time format</legend>
              <div class="radio-options">
                <label class="choice">
                  <input
                    type="radio"
                    name="format"
                    value="12")HTML");

  if (!current.use24HourFormat) page += F(" checked");

  page += F(R"HTML(>
                    <span>12-hour</span>
                  </label>

                  <label class="choice">
                    <input
                      type="radio"
                      name="format"
                      value="24")HTML");

  if (current.use24HourFormat) page += F(" checked");

  page += F(R"HTML(>
                      <span>24-hour</span>
                    </label>
                  </div>
                </fieldset>

                <div class="field">
                  <label class="toggle-row" for="serial">
                    <input
                      type="checkbox"
                      id="serial"
                      name="serial")HTML");

  if (current.serialPrintsEnabled) page += F(" checked");

  page += F(R"HTML(>
                      <span class="toggle-text">
                        Serial logging
                        <span class="toggle-description">
                          Enable diagnostic output over the serial connection.
                        </span>
                      </span>
                    </label>
                  </div>
              </div>

              <div class="tab-panel" id="colors" role="tabpanel">
)HTML");

  appendColorPicker(page, "weatherColor", "Weather status",
                  current.weatherLabelColor);
  appendColorPicker(page, "hourColor", "Clock: HH:MM",
                  current.hourLabelColor);
  appendColorPicker(page, "secondColor", "Clock: Seconds",
                  current.secondLabelColor);
  appendColorPicker(page, "dateColor", "Date",
                  current.dateLabelColor);
  appendColorPicker(page, "dayColor", "Day",
                  current.dayLabelColor);
  appendColorPicker(page, "progressColor", "Seconds ring",
                  current.secondProgressColor);

  page += F(R"HTML(
              </div>

              <div class="tab-panel" id="position" role="tabpanel">
)HTML");

  appendPositionControl(page, "weatherIcon", "Weather icon",
                      current.weatherIconX, current.weatherIconY);
  appendPositionControl(page, "weatherLabel", "Weather status",
                      current.weatherLabelX, current.weatherLabelY);
  appendPositionControl(page, "hourLabel", "Clock: HH:MM",
                      current.hourLabelX, current.hourLabelY);
  appendPositionControl(page, "secondLabel", "Clock: Seconds",
                      current.secondLabelX, current.secondLabelY);
  appendPositionControl(page, "dateLabel", "Date",
                      current.dateLabelX, current.dateLabelY);
  appendPositionControl(page, "dayLabel", "Day",
                      current.dayLabelX, current.dayLabelY);

  page += F(R"HTML(
              </div>

              <button type="submit">Save settings</button>
            </form>
            <form method="post" action="/settings/reload"
                  onsubmit="return confirm('Reload the configuration currently saved on this device? Any unsaved changes on this page will be discarded.');">
              <button type="submit" class="secondary-button">
                Load saved configuration
              </button>
            </form>
          </section>

      <section>
        <details>
          <summary><h2>Login Credentials</h2></summary>

          <form method="post" action="/password">
            <div class="field">
              <label class="field-label" for="currentPassword">
                Current password
              </label>
              <input
                type="password"
                id="currentPassword"
                name="currentPassword"
                autocomplete="current-password"
                required
              >
            </div>

            <div class="field">
              <label class="field-label" for="password">
                New password
              </label>
              <input
                type="password"
                id="password"
                name="password"
                minlength="8"
                maxlength="64"
                autocomplete="new-password"
                required
              >
              <span class="hint">Use 8–64 characters.</span>
            </div>

            <div class="field">
              <label class="field-label" for="confirm">
                Confirm new password
              </label>
              <input
                type="password"
                id="confirm"
                name="confirm"
                minlength="8"
                maxlength="64"
                autocomplete="new-password"
                required
              >
            </div>

            <button type="submit" class="secondary-button">
              Update password
            </button>
          </form>
        </details>
      </section>

      <footer>
        ESP32 Clock · Device configuration<br>
        Created by <a href="https://github.com/amarnathmb"
                    target="_blank"
                    rel="noopener noreferrer">Amarnath MB</a>
      </footer>
    </main>

    <script>
      const tabButtons = document.querySelectorAll('.tab-button');
      const tabPanels = document.querySelectorAll('.tab-panel');

      /* Activates one settings tab and hides the other panels. */
      function activateTab(tabName) {
        tabButtons.forEach((button) => {
          const active = button.dataset.tab === tabName;
          button.classList.toggle('active', active);
          button.setAttribute('aria-selected', String(active));
        });

        tabPanels.forEach((panel) => {
          panel.classList.toggle('active', panel.id === tabName);
        });
      }

      tabButtons.forEach((button) => {
        button.addEventListener('click', () => {
          activateTab(button.dataset.tab);
        });
      });
    </script>
  </body>
  </html>
  )HTML");
return page;
}

/* Serves the settings page after authenticating the request. */
static void handleSettingsPage()
{
  if (!authenticateWebRequest()) return;
  server.send(HTTP_CODE_OK, "text/html; charset=utf-8", settingsPage());
}

/* Validates, persists, and applies submitted settings. */
static void handleSettingsSave()
{
  if (!authenticateWebRequest()) return;

  if (!settingsStorageReady()) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Settings storage is unavailable.");
    return;
  }

  if (!server.hasArg("interval") || !server.hasArg("format") ||
      !server.hasArg("latitude") || !server.hasArg("longitude") ||
      !server.hasArg("weatherIconX") || !server.hasArg("weatherIconY") ||
      !server.hasArg("weatherLabelX") || !server.hasArg("weatherLabelY") ||
      !server.hasArg("hourLabelX") || !server.hasArg("hourLabelY") ||
      !server.hasArg("secondLabelX") || !server.hasArg("secondLabelY") ||
      !server.hasArg("dateLabelX") || !server.hasArg("dateLabelY") ||
      !server.hasArg("dayLabelX") || !server.hasArg("dayLabelY")) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing required settings.");
    return;
  }

  String intervalText = server.arg("interval");
  uint32_t intervalSeconds = 0;
  for (size_t i = 0; i < intervalText.length(); ++i) {
    char digit = intervalText[i];
    if (digit < '0' || digit > '9') {
      server.send(HTTP_CODE_BAD_REQUEST, "text/plain", "Interval must be a number.");
      return;
    }

    uint32_t numericDigit = static_cast<uint32_t>(digit - '0');
    if (intervalSeconds >
        (MAX_WEATHER_INTERVAL_SECONDS - numericDigit) / 10) {
      server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Interval must be between 10 and 3600 seconds.");
      return;
    }
    intervalSeconds = intervalSeconds * 10 + numericDigit;
  }

  if (intervalSeconds < MIN_WEATHER_INTERVAL_SECONDS ||
      intervalSeconds > MAX_WEATHER_INTERVAL_SECONDS) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Interval must be between 10 and 3600 seconds.");
    return;
  }

  String format = server.arg("format");
  if (format != "12" && format != "24") {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain", "Time format must be 12 or 24.");
    return;
  }

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

  if (!parseIntegerInput(server.arg("weatherIconX"), weatherIconX, -120, 120) ||
      !parseIntegerInput(server.arg("weatherIconY"), weatherIconY, -120, 120) ||
      !parseIntegerInput(server.arg("weatherLabelX"), weatherLabelX, -120, 120) ||
      !parseIntegerInput(server.arg("weatherLabelY"), weatherLabelY, -120, 120) ||
      !parseIntegerInput(server.arg("hourLabelX"), hourLabelX, -120, 120) ||
      !parseIntegerInput(server.arg("hourLabelY"), hourLabelY, -120, 120) ||
      !parseIntegerInput(server.arg("secondLabelX"), secondLabelX, -120, 120) ||
      !parseIntegerInput(server.arg("secondLabelY"), secondLabelY, -120, 120) ||
      !parseIntegerInput(server.arg("dateLabelX"), dateLabelX, -120, 120) ||
      !parseIntegerInput(server.arg("dateLabelY"), dateLabelY, -120, 120) ||
      !parseIntegerInput(server.arg("dayLabelX"), dayLabelX, -120, 120) ||
      !parseIntegerInput(server.arg("dayLabelY"), dayLabelY, -120, 120)) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Layout offsets must be integers between -120 and 120.");
    return;
  }

  double latitude;
  double longitude;
  if (!parseCoordinate(server.arg("latitude"), -90.0, 90.0, latitude) ||
      !parseCoordinate(server.arg("longitude"), -180.0, 180.0, longitude)) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Latitude must be -90 to 90 and longitude -180 to 180.");
    return;
  }

  const char* colorFields[] = {
    "weatherColor", "hourColor", "secondColor",
    "dateColor", "dayColor", "progressColor"
  };
  uint32_t colors[6];
  for (size_t i = 0; i < 6; ++i) {
    if (!server.hasArg(colorFields[i]) ||
        !parseColorInput(server.arg(colorFields[i]), colors[i])) {
      server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Each display color must be a valid #RRGGBB value.");
      return;
    }
  }

  ClockSettings updated = {
    intervalSeconds * 1000UL,
    server.hasArg("serial"),
    format == "24",
    colors[0],
    colors[1],
    colors[2],
    colors[3],
    colors[4],
    colors[5],
    weatherIconX,
    weatherIconY,
    weatherLabelX,
    weatherLabelY,
    hourLabelX,
    hourLabelY,
    secondLabelX,
    secondLabelY,
    dateLabelX,
    dateLabelY,
    dayLabelX,
    dayLabelY,
    latitude,
    longitude
  };

  if (!saveSettings(updated)) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Could not save settings to flash.");
    return;
  }

  applyConfiguredColors();
  applyConfiguredLayout();

  server.sendHeader("Location", "/");
  server.send(HTTP_CODE_SEE_OTHER, "text/plain", "Settings saved.");
}

/* Reloads persisted settings and applies them to the active display. */
static void handleSettingsReload()
{
  if (!authenticateWebRequest()) return;

  if (!settingsStorageReady()) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Settings storage is unavailable.");
    return;
  }

  if (!reloadSavedSettings()) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Could not load saved settings.");
    return;
  }

  applyConfiguredColors();
  applyConfiguredLayout();

  server.sendHeader("Location", "/");
  server.send(HTTP_CODE_SEE_OTHER, "text/plain",
              "Saved settings loaded.");
}

/* Verifies the current password and stores a validated replacement. */
static void handlePasswordChange()
{
  if (!authenticateWebRequest()) return;

  if (!settingsStorageReady()) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Settings storage is unavailable.");
    return;
  }

  if (!server.hasArg("currentPassword") ||
      !server.hasArg("password") ||
      !server.hasArg("confirm")) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Current password, new password, and confirmation are required.");
    return;
  }

  if (server.arg("currentPassword") != getWebPassword()) {
    server.send(HTTP_CODE_UNAUTHORIZED, "text/plain",
                "Current password is incorrect.");
    return;
  }

  String newPassword = server.arg("password");
  if (newPassword.length() < MIN_WEB_PASSWORD_LENGTH ||
      newPassword.length() > MAX_WEB_PASSWORD_LENGTH) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Password must be between 8 and 64 characters.");
    return;
  }

  if (newPassword != server.arg("confirm")) {
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain",
                "Password confirmation does not match.");
    return;
  }

  if (!saveWebPassword(newPassword)) {
    server.send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain",
                "Could not save the new password.");
    return;
  }

  server.send(HTTP_CODE_OK, "text/plain",
              "Password updated. Use the new password for your next login.");
}

/* Registers web routes and starts the HTTP settings server. */
void startSettingsServer()
{
  server.on("/", HTTP_GET, handleSettingsPage);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  server.on("/settings/reload", HTTP_POST, handleSettingsReload);
  server.on("/password", HTTP_POST, handlePasswordChange);
  server.onNotFound([]() {
    if (!authenticateWebRequest()) return;
    server.send(HTTP_CODE_NOT_FOUND, "text/plain", "Not found.");
  });
  server.begin();
}


/* Handles one pending web-server client request. */
void handleSettingsServerClient()
{
  server.handleClient();
}