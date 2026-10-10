#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#else
#include "arduino_secrets_example.h"
#endif

// All project-wide constants in one place. Values target a Fronius inverter
// on the local network and a boiler switched above 5000 W of production.
namespace config {

// WiFi credentials (from arduino_secrets.h, gitignored)
constexpr const char* WIFI_SSID = SECRET_SSID;
constexpr const char* WIFI_PASS = SECRET_PASS;

// Inverter (Fronius Solar API v1, plain HTTP, no authentication)
constexpr uint8_t INVERTER_IP[4] = {192, 168, 1, 161};
constexpr uint16_t HTTP_TIMEOUT_MS = 5000;
constexpr size_t HTTP_MAX_BODY_BYTES = 4096; // bounded: SAMD21 has 32 KB RAM
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Switch / boiler
constexpr uint8_t SWITCH_PIN = 6;             // 3.3 V signal pin (see README)
constexpr int POWER_ON_THRESHOLD_W = 5000;    // boiler ON at/above
constexpr int POWER_OFF_THRESHOLD_W = 4500;   // boiler OFF at/below (hysteresis)
constexpr uint32_t MIN_ON_TIME_MS = 300000;   // 5 min: protect relay + boiler
constexpr uint32_t MIN_OFF_TIME_MS = 300000;   // 5 min
constexpr uint8_t FAILURES_BEFORE_FORCED_OFF = 5; // ~5 polls without any
                                                  // valid reading -> OFF

// Polling
constexpr uint32_t DELAY_BETWEEN_CHECKS_MS = 90000; // 90 s

// Night sleep
constexpr int SLEEP_START_HOUR = 21;          // local hour, inclusive
constexpr uint32_t NIGHT_SLEEP_DURATION_MS = 32400000UL; // 9 h

// Time synchronization (SNTP)
constexpr const char* NTP_SERVER = "pool.ntp.org";
constexpr long UTC_OFFSET_SECONDS = 3600;     // CET; DST shifts by 1 h,
                                              // tolerated by the 21 h bound
constexpr uint32_t NTP_SYNC_INTERVAL_S = 86400;      // daily, RTC-based
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 1800000;  // 30 min while unsynced
constexpr uint32_t NTP_TIMEOUT_MS = 5000;

// Fail-safe when the network cannot be (re)joined: low-power wait before
// retrying, so a dead AP does not drain the board with busy retries.
constexpr uint32_t NETWORK_FAILURE_RETRY_MS = 600000; // 10 min

} // namespace config

#endif
