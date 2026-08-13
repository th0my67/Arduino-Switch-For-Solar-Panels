#pragma once
#include <Arduino.h>
#include <WiFiNINA.h>

#include "../core/Ports.h"
#include "../hal/INetwork.h"
#include "../hal/IStatusLed.h"

namespace solar {

// MKR WiFi 1010 (NINA-W102) adapter.
class WiFiNetwork : public INetwork {
 public:
  WiFiNetwork(const char* ssid, const char* password, IStatusLed& led)
      : ssid_(ssid), password_(password), led_(led) {}

  bool begin() override {
    while (WiFi.begin(ssid_, password_) != WL_CONNECTED) {
      led_.signal(LedEvent::LinkError);
      delay(4000);
    }
    return true;
  }

  bool ensureUp() override {
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.end();
    return begin();
  }

 private:
  const char* ssid_;
  const char* password_;
  IStatusLed& led_;
};

// NTP time through the NINA module. Used when the inverter API carries no
// usable timestamp (e.g. SolarEdge). WiFi.getTime() returns UTC, so a fixed
// offset converts it to local time — unlike the Fronius timestamp, DST
// changes require updating the offset (or moving to a real timezone lib).
class NinaTimeSource : public ITimeSource {
 public:
  explicit NinaTimeSource(int32_t utcOffsetSeconds)
      : utcOffsetSeconds_(utcOffsetSeconds) {}

  bool readLocalEpoch(uint32_t& epoch) override {
    const uint32_t utc = WiFi.getTime();
    if (utc == 0) return false;  // NTP not synced yet
    epoch = utc + (uint32_t)utcOffsetSeconds_;
    return true;
  }

 private:
  int32_t utcOffsetSeconds_;
};

}  // namespace solar
