#include "FroniusClient.h"

#include "../core/JsonScanner.h"
#include "../core/TimeUtils.h"

namespace solar {

bool FroniusClient::readPowerWatts(int32_t& watts) {
  IByteReader* body =
      http_.get(cfg_.host, cfg_.port, cfg_.pathAndQuery, false, nullptr);
  if (!body) return false;
  JsonScanner json(*body);
  float value = 0.0f;
  const bool ok =
      json.findKey("PAC") && json.findKey("Value") && json.readFloat(value);
  http_.close();
  if (ok) watts = (int32_t)value;
  return ok;
}

bool FroniusClient::readLocalEpoch(uint32_t& epoch) {
  IByteReader* body =
      http_.get(cfg_.host, cfg_.port, cfg_.pathAndQuery, false, nullptr);
  if (!body) return false;
  JsonScanner json(*body);
  char buf[26];  // "2024-04-08T13:45:30+02:00" + NUL
  const bool ok =
      json.findKey("Timestamp") && json.readString(buf, sizeof(buf));
  http_.close();
  return ok && timeutil::parseIsoLocal(buf, epoch);
}

}  // namespace solar
