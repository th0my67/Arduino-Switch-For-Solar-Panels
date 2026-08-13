#pragma once
#include <stdint.h>

#include "PlatformSelect.h"

// All user-tunable settings live here. Hardware selection is in
// PlatformSelect.h; WiFi credentials are in Secrets.h (gitignored).
namespace config {

// ---- Hardware -------------------------------------------------------------
constexpr uint8_t kSwitchPin = 6;
// Replace with the MAC address printed on your MKR ETH Shield sticker.
constexpr uint8_t kEthernetMac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// ---- Inverter -------------------------------------------------------------
// Vendor selection is in PlatformSelect.h (SOLAR_INVERTER_*). Each vendor is
// a small adapter in src/services implementing the IPowerReader port; adding
// another vendor means writing one more adapter, nothing else changes.
#if SOLAR_INVERTER_SOLAREDGE
#include "Secrets.h"  // SOLAREDGE_ACCOUNT_KEY / SOLAREDGE_API_KEY
constexpr char kInverterHost[] = "monitoringapi.solaredge.com";
constexpr uint16_t kInverterPort = 443;
constexpr bool kInverterUseTls = true;
constexpr char kInverterPath[] = "/v2/sites/YOUR_SITE_ID/power-flow";
constexpr char kInverterHeaders[] =
    "X-ACCOUNT-KEY: " SOLAREDGE_ACCOUNT_KEY "\r\n"
    "X-API-KEY: " SOLAREDGE_API_KEY "\r\n"
    "Accept: application/json, application/problem+json\r\n";
// The power-flow endpoint reports pv power; set to 1000.0f if your site
// returns kilowatts instead of watts.
constexpr float kSolarEdgePowerScaleToWatts = 1.0f;
// SolarEdge's API has no wall-clock timestamp, so time comes from NTP (UTC):
// this offset converts it to local time. Update it on DST changes.
constexpr int32_t kUtcOffsetSeconds = 2 * 3600;  // CEST
#else  // Fronius Solar API v1 (local network)
constexpr char kInverterHost[] = "192.168.1.170";
constexpr uint16_t kInverterPort = 80;
constexpr char kInverterPath[] =
    "/solar_api/v1/GetInverterRealtimeData.cgi"
    "?Scope=Device&DeviceId=1&DataCollection=CumulationInverterData";
#endif

// ---- Day behaviour --------------------------------------------------------
constexpr int32_t kPowerOnThresholdW = 5000;   // close the switch at/above
constexpr int32_t kPowerOffThresholdW = 4500;  // open it below (hysteresis)
constexpr uint32_t kDayPollIntervalS = 90;

// ---- Day/night schedule (hours, local time) -------------------------------
constexpr uint8_t kEveningHour = 21;  // the forecast decision runs here
constexpr uint8_t kMorningHour = 6;   // daytime polling resumes here

// ---- Night boost ----------------------------------------------------------
// At kEveningHour the controller fetches tomorrow's sunshine forecast. If it
// is below kMinForecastSunshineHours AND the switch was ON for less than
// kMaxPastActivationHours over the last kPastDaysWindow days, the switch is
// forced ON at kBoostStartHour for kBoostDurationS (e.g. cheap night tariff).
constexpr uint8_t kBoostStartHour = 2;
constexpr uint32_t kBoostDurationS = 3 * 3600UL;
constexpr float kMinForecastSunshineHours = 4.0f;
constexpr float kMaxPastActivationHours = 6.0f;
constexpr uint8_t kPastDaysWindow = 2;  // today + yesterday
constexpr bool kBoostWhenForecastUnavailable = false;

// ---- Weather forecast -----------------------------------------------------
// MeteoSwiss ICON-CH1 model served as JSON by Open-Meteo (the official
// MeteoSwiss open data is GRIB2 — unrealistic to parse on an MCU).
// Set latitude/longitude to your location.
constexpr char kWeatherHost[] = "api.open-meteo.com";
constexpr char kWeatherPath[] =
    "/v1/forecast?latitude=46.5197&longitude=6.6323"
    "&daily=sunshine_duration&models=icon_ch1"
    "&timezone=Europe%2FZurich&forecast_days=2";
#if SOLAR_PLATFORM_MKR_WIFI
constexpr bool kWeatherUseTls = true;   // WiFiNINA ships TLS + root certs
#else
constexpr bool kWeatherUseTls = false;  // W5500 has no TLS; plain HTTP
#endif
constexpr uint16_t kWeatherPort = kWeatherUseTls ? 443 : 80;

// ---- System ---------------------------------------------------------------
constexpr uint32_t kClockResyncIntervalS = 6 * 3600UL;  // re-read inverter time
constexpr uint32_t kDeepSleepThresholdS = 15 * 60UL;    // longer waits deep-sleep

}  // namespace config
