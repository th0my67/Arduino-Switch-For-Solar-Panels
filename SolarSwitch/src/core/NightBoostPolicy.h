#pragma once
#include <stdint.h>

namespace solar {

struct NightBoostConfig {
  float minForecastSunshineHours;  // boost only if tomorrow's sunshine is below this
  float maxPastActivationHours;    // ...and recent activation stayed below this
  uint8_t pastDaysWindow;          // how many days (incl. today) count as "recent"
  bool boostWhenForecastUnavailable;
};

struct NightBoostInputs {
  bool forecastValid;
  float forecastSunshineHours;
  float pastActivationHours;
};

// Pure decision function — trivial to unit test and to evolve.
inline bool shouldNightBoost(const NightBoostConfig& cfg,
                             const NightBoostInputs& in) {
  if (in.pastActivationHours >= cfg.maxPastActivationHours) return false;
  if (!in.forecastValid) return cfg.boostWhenForecastUnavailable;
  return in.forecastSunshineHours < cfg.minForecastSunshineHours;
}

}  // namespace solar
