#include "SolarEdgeClient.h"

#include "../core/JsonScanner.h"

namespace solar {

bool SolarEdgeClient::readPowerWatts(int32_t& watts) {
  IByteReader* body = http_.get(cfg_.host, cfg_.port, cfg_.pathAndQuery,
                                cfg_.useTls, cfg_.headerLines);
  if (!body) return false;

  JsonScanner json(*body);
  float value = 0.0f;
  const bool ok =
      json.findKey("pv") && json.findKey("power") && json.readFloat(value);
  http_.close();

  if (ok) watts = (int32_t)(value * cfg_.powerScaleToWatts);
  return ok;
}

}  // namespace solar
