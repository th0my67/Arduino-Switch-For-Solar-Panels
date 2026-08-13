#pragma once
#include <stdint.h>

#include "../hal/IClock.h"
#include "../hal/ISwitchOutput.h"
#include "ActivationHistory.h"
#include "NightBoostPolicy.h"
#include "Ports.h"

namespace solar {

struct ControllerConfig {
  uint32_t dayPollIntervalS;   // seconds between inverter polls during the day
  int32_t powerOnThresholdW;   // switch closes at/above this production
  int32_t powerOffThresholdW;  // switch opens below this production (hysteresis)
  uint8_t eveningHour;         // start of the night window (0-23)
  uint8_t morningHour;         // end of the night window (0-23)
  uint8_t boostStartHour;      // night activation start (inside the night window)
  uint32_t boostDurationS;     // how long the night activation lasts
  NightBoostConfig boost;
};

// The application's brain: a small state machine, fully hardware-independent.
//
//   Day        — poll the inverter, drive the switch from produced power.
//   NightIdle  — evening decision taken; waiting for the boost or the morning.
//   NightBoost — switch forced ON for the configured night slot.
//
// tick() performs one step and returns how many seconds the caller should
// sleep before the next call (0 = call again immediately).
class SolarSwitchController {
 public:
  enum class Mode : uint8_t { Day, NightIdle, NightBoost };

  SolarSwitchController(const ControllerConfig& cfg, IClock& clock,
                        ISwitchOutput& switchOutput, IPowerReader& power,
                        ISunshineForecast& forecast,
                        ActivationHistory& history);

  uint32_t tick();

  // Introspection for logging and tests.
  Mode mode() const { return mode_; }
  bool boostPlanned() const { return boostPlanned_; }
  bool lastForecastValid() const { return lastForecastValid_; }
  float lastForecastHours() const { return lastForecastHours_; }
  float lastPastActivationHours() const { return lastPastActivationHours_; }

 private:
  bool isNight(uint8_t hour) const;
  uint32_t dayTick(uint32_t now);
  uint32_t eveningDecision(uint32_t now);
  uint32_t nightIdleTick(uint32_t now, uint8_t hour);
  uint32_t endBoost(uint32_t now);

  ControllerConfig cfg_;
  IClock& clock_;
  ISwitchOutput& switch_;
  IPowerReader& power_;
  ISunshineForecast& forecast_;
  ActivationHistory& history_;

  Mode mode_;
  bool boostPlanned_;
  uint32_t boostStartEpoch_;
  bool lastForecastValid_;
  float lastForecastHours_;
  float lastPastActivationHours_;
};

}  // namespace solar
