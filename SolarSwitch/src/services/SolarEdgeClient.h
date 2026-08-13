#pragma once
#include <stdint.h>

#include "../core/Ports.h"
#include "../hal/IHttpClient.h"

namespace solar {

struct SolarEdgeConfig {
  const char* host;
  uint16_t port;
  const char* pathAndQuery;   // e.g. "/v2/sites/<SITE_ID>/power-flow"
  const char* headerLines;    // auth headers, each line ending with "\r\n"
  bool useTls;                // the real monitoring API requires TLS
  float powerScaleToWatts;    // 1.0 if the API reports W, 1000.0 for kW
};

// Adapter for the SolarEdge monitoring API v2 power-flow endpoint.
// Extracts "pv" -> "power". Demonstrates that swapping the inverter vendor
// only means implementing IPowerReader — the core logic is untouched.
// Note: this API does not expose a wall-clock timestamp, so it does not
// implement ITimeSource; pair it with an NTP-based source (see WiFiPlatform).
class SolarEdgeClient : public IPowerReader {
 public:
  SolarEdgeClient(IHttpClient& http, const SolarEdgeConfig& cfg)
      : http_(http), cfg_(cfg) {}

  bool readPowerWatts(int32_t& watts) override;

 private:
  IHttpClient& http_;
  SolarEdgeConfig cfg_;
};

}  // namespace solar
