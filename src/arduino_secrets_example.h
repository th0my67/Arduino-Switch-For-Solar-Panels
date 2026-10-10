#ifndef ARDUINO_SECRETS_EXAMPLE_H
#define ARDUINO_SECRETS_EXAMPLE_H

// Safe placeholders. Copy this file to src/arduino_secrets.h (gitignored)
// and fill in the credentials for the inverter brand you selected in
// config.h. Unused entries can stay as placeholders.

// WiFi
#define SECRET_SSID "your-wifi-ssid"
#define SECRET_PASS "your-wifi-password"

// SolarEdge Monitoring API v2 (cloud):
// https://monitoringapi.solaredge.com -> site details -> API access
#define SECRET_SOLAREDGE_SITE_ID "1234567"
#define SECRET_SOLAREDGE_API_KEY "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#endif
