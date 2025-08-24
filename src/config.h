#ifndef CONFIG_H
#define CONFIG_H

#include "arduino_secrets.h"

// Network Configuration
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// API Configuration
extern const char* API_HOST;
extern const int API_PORT;
extern const char* API_PATH;

// Hardware Configuration
extern const int SWITCH_PIN;
extern const int POWER_LIMIT_W;

// Timing Configuration
extern const int DELAY_BETWEEN_CHECKS_MS;
extern const int REQUESTS_BEFORE_TIME_CHECK;
extern const int SLEEP_START_HOUR;
extern const int NIGHT_SLEEP_DURATION_MS;

// Debug Configuration
extern const bool DEBUG_SERIAL_ENABLED;
extern const bool DEBUG_LED_ENABLED;

#endif