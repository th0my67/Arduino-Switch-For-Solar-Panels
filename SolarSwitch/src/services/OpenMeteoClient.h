#pragma once
#include <stdint.h>

#include "../core/Ports.h"
#include "../hal/IHttpClient.h"

namespace solar {

struct WeatherConfig {
  const char* host;
  uint16_t port;
  const char* pathAndQuery;
  bool useTls;
};

// Sunshine forecast from the MeteoSwiss ICON-CH1 model, served as JSON by
// Open-Meteo. The official MeteoSwiss open data distribution (STAC/GRIB2)
// is far too heavy for a microcontroller; Open-Meteo redistributes the same
// model output as a small JSON document. Expected query (see AppConfig.h):
//   /v1/forecast?latitude=..&longitude=..&daily=sunshine_duration
//   &models=icon_ch1&timezone=Europe%2FZurich&forecast_days=2
class OpenMeteoClient : public ISunshineForecast {
 public:
  OpenMeteoClient(IHttpClient& http, const WeatherConfig& cfg)
      : http_(http), cfg_(cfg) {}

  bool readSunshineHoursTomorrow(float& hours) override;

 private:
  IHttpClient& http_;
  WeatherConfig cfg_;
};

}  // namespace solar
