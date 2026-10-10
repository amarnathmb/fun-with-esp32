#pragma once

#include "ClockSettings.h"

#define SERIAL_LOG_PRINT(...) \
  do { if (getSettings().serialPrintsEnabled) Serial.print(__VA_ARGS__); } while (0)
#define SERIAL_LOG_PRINTLN(...) \
  do { if (getSettings().serialPrintsEnabled) Serial.println(__VA_ARGS__); } while (0)
#define SERIAL_LOG_PRINTF(...) \
  do { if (getSettings().serialPrintsEnabled) Serial.printf(__VA_ARGS__); } while (0)
