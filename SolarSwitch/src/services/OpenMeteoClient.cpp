#include "OpenMeteoClient.h"

#include "../core/JsonScanner.h"

namespace solar {

bool OpenMeteoClient::readSunshineHoursTomorrow(float& hours) {
  IByteReader* body =
      http_.get(cfg_.host, cfg_.port, cfg_.pathAndQuery, cfg_.useTls, nullptr);
  if (!body) return false;

  JsonScanner json(*body);
  float values[2] = {0.0f, 0.0f};
  uint8_t count = 0;
  // The response also contains "daily_units" (where sunshine_duration is the
  // string "s"); entering "daily" first skips it — findKey matches the exact
  // quoted key, so "daily" cannot match inside "daily_units".
  const bool ok = json.findKey("daily") && json.findKey("sunshine_duration") &&
                  json.readFloatArray(values, 2, count) && count >= 2;
  http_.close();

  if (ok) hours = values[1] / 3600.0f;  // seconds per day; [0]=today, [1]=tomorrow
  return ok;
}

}  // namespace solar
