#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "services/activation_tracker.h"

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

// --- Inverter brand selection -------------------------------------------
// FRONIUS:    local Solar API v1 over HTTP (needs INVERTER_IP)
// SOLAREDGE:  cloud Monitoring API v2 over HTTPS (needs site id + API key,
//             the API key comes from arduino_secrets.h)
#define INVERTER_BRAND_FRONIUS 1
#define INVERTER_BRAND_SOLAREDGE 2
#ifndef INVERTER_BRAND
#define INVERTER_BRAND INVERTER_BRAND_FRONIUS
#endif

// Fronius (local network, plain HTTP, no authentication)
constexpr uint8_t INVERTER_IP[4] = {192, 168, 1, 161};
constexpr uint16_t HTTP_TIMEOUT_MS = 5000;
constexpr size_t HTTP_MAX_BODY_BYTES = 4096; // bounded: SAMD21 has 32 KB RAM
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// SolarEdge (cloud Monitoring API v2)
#ifndef SECRET_SOLAREDGE_SITE_ID
#define SECRET_SOLAREDGE_SITE_ID "1234567"
#endif
#ifndef SECRET_SOLAREDGE_API_KEY
#define SECRET_SOLAREDGE_API_KEY "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#endif
constexpr const char* SOLAREDGE_SITE_ID = SECRET_SOLAREDGE_SITE_ID;
constexpr const char* SOLAREDGE_API_KEY = SECRET_SOLAREDGE_API_KEY;
constexpr const char* SOLAREDGE_HOST = "monitoringapi.solaredge.com";

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

// --- Backup grid charge (anti-cold-water) --------------------------------
// If the switch has been ON less than BACKUP_MIN_ACTIVATION_H during the
// last BACKUP_HISTORY_DAYS days AND tomorrow's sunshine forecast is below
// BACKUP_SUNSHINE_THRESHOLD_H, the boiler is charged from the grid the
// next day, from BACKUP_START_HOUR for BACKUP_CHARGE_DURATION_H.
// The decision is evaluated daily at WEATHER_FETCH_HOUR (local).
constexpr bool BACKUP_ENABLED = true;
constexpr float BACKUP_MIN_ACTIVATION_H = 2.0;
constexpr uint8_t BACKUP_HISTORY_DAYS = 2;
constexpr float BACKUP_SUNSHINE_THRESHOLD_H = 4.0;
constexpr uint8_t BACKUP_START_HOUR = 13;
constexpr uint8_t BACKUP_CHARGE_DURATION_H = 3;
constexpr uint8_t WEATHER_FETCH_HOUR = 18; // after solar peak, before night

// Sunshine forecast (Open-Meteo; MeteoSwiss publishes its sunshine
// forecast only as multi-MB all-locations files, unusable on this board).
constexpr double WEATHER_LATITUDE = 46.5;  // your location
constexpr double WEATHER_LONGITUDE = 6.6;
constexpr const char* WEATHER_TIMEZONE = "Europe/Zurich";

// Fail-safe when the network cannot be (re)joined: low-power wait before
// retrying, so a dead AP does not drain the board with busy retries.
constexpr uint32_t NETWORK_FAILURE_RETRY_MS = 600000; // 10 min

// --- Compile-time invariants ---------------------------------------------
// A time-related misconfiguration must fail the build, not silently
// misbehave at runtime. Evaluated wherever config.h is included: the
// firmware build (via main.cpp) and the native test build (via
// test_config).
static_assert(SLEEP_START_HOUR >= 1 && SLEEP_START_HOUR <= 23,
    "SLEEP_START_HOUR must be in [1..23]: 0 would sleep forever, >= 24 never matches");
static_assert(BACKUP_START_HOUR <= 23,
    "BACKUP_START_HOUR must be in [0..23]");
static_assert(BACKUP_CHARGE_DURATION_H >= 1,
    "BACKUP_CHARGE_DURATION_H must be at least 1 h");
static_assert(WEATHER_FETCH_HOUR <= 23,
    "WEATHER_FETCH_HOUR must be in [0..23]");
static_assert((long)BACKUP_START_HOUR + (long)BACKUP_CHARGE_DURATION_H <= (long)SLEEP_START_HOUR,
    "the backup charge must end before the night sleep starts: "
    "BACKUP_START_HOUR + BACKUP_CHARGE_DURATION_H <= SLEEP_START_HOUR");
static_assert(WEATHER_FETCH_HOUR < SLEEP_START_HOUR,
    "the daily weather evaluation must run while the board is awake: "
    "WEATHER_FETCH_HOUR < SLEEP_START_HOUR");
static_assert(POWER_OFF_THRESHOLD_W < POWER_ON_THRESHOLD_W,
    "hysteresis requires POWER_OFF_THRESHOLD_W < POWER_ON_THRESHOLD_W");
static_assert(BACKUP_HISTORY_DAYS >= 1 && BACKUP_HISTORY_DAYS <= ActivationTracker::kMaxDays,
    "BACKUP_HISTORY_DAYS must be in [1..8] (ActivationTracker::kMaxDays)");

} // namespace config

#endif
