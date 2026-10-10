# ESP32 GC9A01 Weather Clock

An ESP32 Arduino firmware project for a 240 × 240 GC9A01A round display. It
shows a synchronized clock, date, weekday, seconds progress ring, and current
weather icon and temperature. A browser-based settings page lets you configure
weather location and interval, display colors and positions, time format, and
serial logging.

## Features

- Local time synchronized over NTP (UTC+05:30).
- Weather data and icons from the Open-Meteo forecast API.
- Background weather fetching so network requests do not block LVGL rendering.
- LVGL-based display with configurable colors and position offsets.
- Authenticated web settings and persistent configuration stored in ESP32
  Preferences.
- Settings can be saved and applied without rebooting, or reloaded from flash.

## Repository layout

This README is in the Arduino sketch folder. Open [`LCD_GC9A01.ino`](./LCD_GC9A01.ino)
from this folder in Arduino IDE.
The module paths in the table below are relative to the sketch folder.

| Path | Responsibility |
| --- | --- |
| `LCD_GC9A01.ino` | Startup sequencing and main-loop coordination |
| `src/ClockDisplay.cpp` | GC9A01, LVGL setup, clock labels, and display updates |
| `src/ClockSettings.cpp` | Defaults, validation, and Preferences persistence |
| `src/WeatherService.cpp` | Weather API requests, icon mapping, and background task |
| `src/WebSettings.cpp` | HTTP routes, authentication, settings validation, and page generation |
| `src/SerialLog.h` | Serial logging macros controlled by the saved setting |
| `src/icons/` | Generated LVGL weather image assets |
| `src/fonts/` | Generated Ubuntu LVGL font assets |
| `src/StartupImageConfig.h` | Optional compile-time startup image hook |

The generated icon and font C files are compiled as part of the sketch. Keep
them under the sketch's `src/` directory when adding or replacing assets.

## Hardware

- ESP32 development board (ESP32 Dev Module target)
- GC9A01A 240 × 240 round TFT display

The display driver currently uses these configured values in
`src/ClockDisplay.cpp`:

| Signal | Configuration |
| --- | --- |
| Chip select | GPIO 5 |
| Data/command | GPIO 16 |
| Reset constructor argument | `414` |
| SPI data and clock | Hardware SPI pins for the selected ESP32 board |

Check the reset configuration against your board and display wiring before
connecting hardware. Do not change the board's SPI pin mapping without updating
the hardware wiring accordingly.

## Software requirements

- Arduino IDE or Arduino CLI.
- Espressif ESP32 Arduino core. The supplied project configuration was built
  with core 3.3.12.
- LVGL 9.x. The supplied configuration used LVGL 9.6.0.
- Adafruit GFX Library (tested with 1.12.6).
- Adafruit GC9A01A Library (tested with 1.1.1).
- Adafruit BusIO (tested with 1.17.4).

Wi-Fi, WebServer, Preferences, HTTPClient, and secure-client support are
provided by the ESP32 Arduino core. LVGL's configuration must enable the fonts
used by the sketch, including Montserrat 20; the generated icon assets use
RGB565A8 image data.

## Wi-Fi credentials

The sketch includes `../secrets.h`, which resolves to the repository root.
Copy `secrets.h.example` to `secrets.h` in the repository root and set your own
network credentials:

```cpp
#pragma once

const char* WIFI_SSID = "your-wifi-name";
const char* WIFI_PASSWORD = "your-wifi-password";
```

Do not commit real credentials. Keep `secrets.h` private and use placeholders
when sharing configuration or logs.

## Build and upload

1. Install the ESP32 board package and libraries listed above.
2. Copy the root `secrets.h.example` to `secrets.h` and add your Wi-Fi credentials.
3. Open `LCD_GC9A01.ino` from this folder in Arduino IDE.
4. Select **ESP32 Dev Module** and the serial port for your board.
5. Use the project's board settings: 240 MHz CPU, 80 MHz flash, QIO flash mode,
   4 MB flash, and the `Huge APP` partition scheme. Upload speed is configured
   as 921600 where supported.
6. Compile and upload. The firmware is designed to build from the Arduino IDE
   sketch directory; the custom fonts and icons are included beneath `src/`.

Verify the display wiring and board options in Arduino IDE if compilation or
display initialization fails.

## Startup and operation

On boot, the firmware loads settings from Preferences, initializes the display,
connects to Wi-Fi, starts the settings web server, and requests NTP time. The
public version has no startup image enabled. The display remains clear until
local time is available. To add a startup image locally, put a generated LVGL
image C source under `src/icons/` and define `CLOCK_STARTUP_IMAGE_SYMBOL` in
`src/StartupImageConfig.h` to match its exported descriptor. Keep personal
images out of public commits. Once the clock starts, weather requests run in a
separate FreeRTOS task. The main loop continues servicing LVGL, the web server,
and clock updates.

To use a different startup image, place its generated LVGL image C source under
`src/icons/` and set `CLOCK_STARTUP_IMAGE_SYMBOL` to its exported descriptor
symbol. For example:

```cpp
#define CLOCK_STARTUP_IMAGE_SYMBOL my_startup_image
```

The generated descriptor must use C linkage, as LVGL image converter output
does. Keep personal image assets local if they should not be published.

The settings page URL is printed to Serial after Wi-Fi connects, for example:
`http://<device-ip>/`, when serial logging is enabled. Serial Monitor speed is
115200 baud.

## Web settings

The web server uses HTTP Basic Authentication.

- Default username: `admin`
- Initial password: `clock1234`
- Change the initial password through **Login Credentials** after first login.
- Password changes require the current password and a matching new-password
  confirmation. New passwords must be 8–64 characters.

Routes:

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/` | Show the settings page |
| `POST` | `/settings` | Validate, save, and apply submitted settings |
| `POST` | `/settings/reload` | Reload saved configuration after confirmation |
| `POST` | `/password` | Change the web password |

The **Display & updates** section contains:

- **General:** weather interval, coordinates, 12/24-hour format, and serial
  logging.
- **Colors:** configurable weather, clock, seconds, date, weekday, and seconds
  ring colors.
- **Position:** horizontal and vertical offsets for the weather icon, weather
  status, clock, seconds, date, and weekday objects. The accepted offsets are
  −120 to +120 pixels.

Saving applies display settings immediately and stores them in flash.
**Load saved configuration** discards unsaved form edits and reloads the
currently persisted settings; it does not restore factory defaults. Changing
the weather interval affects the background task's next wait cycle.

## Configuration defaults

| Setting | Default | Allowed values / notes |
| --- | --- | --- |
| Weather update interval | 10 seconds | 10–3600 seconds |
| Latitude | `12.964951` | −90 to +90 |
| Longitude | `77.596598` | −180 to +180 |
| Time display | 12-hour | 12-hour or 24-hour |
| Serial logging | Enabled | On/off |
| Weather status color | `#0080F8` | RGB color |
| Clock (`HH:MM`) color | `#FFFFFF` | RGB color |
| Seconds color | `#FF0000` | RGB color |
| Date color | `#00F0B5` | RGB color |
| Weekday color | `#EF476F` | RGB color |
| Seconds ring color | `#FFB400` | RGB color |
| Weather icon position | `(0, -80)` | Relative to display center |
| Weather status position | `(0, -45)` | Relative to display center |
| Clock position | `(-17, 0)` | Relative to display center |
| Seconds position | `(65, 5)` | Relative to display center |
| Date position | `(0, 52)` | Relative to display center |
| Weekday position | `(0, 75)` | Relative to display center |

Settings are stored in the Preferences namespace `clockcfg`. Existing keys
include `interval`, `serial`, `24hour`, `wcolor`, `hcolor`, `scolor`, `dcolor`,
`daycolor`, `pcolor`, `latitude`, `longitude`, and the position keys
`wix`/`wiy`, `wlx`/`wly`, `hlx`/`hly`, `slx`/`sly`, `dlx`/`dly`, and
`daylx`/`dayly`. The web password is stored as `webpass`.

## Security and network notes

- The settings interface uses plain HTTP and Basic Authentication. Use it only
  on a trusted network; do not expose the device's web server directly to the
  public internet.
- The weather client currently disables TLS certificate verification for the
  public Open-Meteo endpoint.
- Wi-Fi credentials and the stored web password are device configuration
  secrets; protect access to the device and its flash storage.
