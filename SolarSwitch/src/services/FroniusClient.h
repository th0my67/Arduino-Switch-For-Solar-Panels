#pragma once
#include <stdint.h>

#include "../core/Ports.h"
#include "../hal/IHttpClient.h"

namespace solar {

struct FroniusConfig {
  const char* host;
  uint16_t port;
  const char* pathAndQuery;
};

// Adapter for the Fronius Solar API v1 realtime endpoint (local, HTTP).
class FroniusClient : public IPowerReader, public ITimeSource {
 public:
  FroniusClient(IHttpClient& http, const FroniusConfig& cfg)
      : http_(http), cfg_(cfg) {}

  // Extracts "PAC" -> "Value" (current AC production in watts).
  bool readPowerWatts(int32_t& watts) override;

  // Extracts "Timestamp" from the response head. The inverter's clock is
  // already local time (with DST handled by the inverter), which makes it a
  // convenient time reference needing no NTP.
  bool readLocalEpoch(uint32_t& epoch) override;

 private:
  IHttpClient& http_;
  FroniusConfig cfg_;
};

}  // namespace solar
