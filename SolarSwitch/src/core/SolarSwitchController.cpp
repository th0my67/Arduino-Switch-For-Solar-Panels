#include "SolarSwitchController.h"

#include "TimeUtils.h"

namespace solar {

SolarSwitchController::SolarSwitchController(
    const ControllerConfig& cfg, IClock& clock, ISwitchOutput& switchOutput,
    IPowerReader& power, ISunshineForecast& forecast, ActivationHistory& history)
    : cfg_(cfg),
      clock_(clock),
      switch_(switchOutput),
      power_(power),
      forecast_(forecast),
      history_(history),
      mode_(Mode::Day),
      boostPlanned_(false),
      boostStartEpoch_(0),
      lastForecastValid_(false),
      lastForecastHours_(0.0f),
      lastPastActivationHours_(0.0f) {}

bool SolarSwitchController::isNight(uint8_t hour) const {
  return hour >= cfg_.eveningHour || hour < cfg_.morningHour;
}

uint32_t SolarSwitchController::tick() {
  const uint32_t now = clock_.now();
  const uint8_t hour = timeutil::hourOfDay(now);

  switch (mode_) {
    case Mode::Day:
      if (isNight(hour)) {
        if (hour < cfg_.morningHour) {
          // Booting in the middle of the night: no evening decision was
          // taken, so just stay off until the morning.
          switch_.set(false);
          history_.onSwitchState(false, now);
          mode_ = Mode::NightIdle;
          return timeutil::secondsUntilHour(now, cfg_.morningHour);
        }
        return eveningDecision(now);
      }
      return dayTick(now);

    case Mode::NightIdle:
      return nightIdleTick(now, hour);

    case Mode::NightBoost:
      return endBoost(now);
  }
  return cfg_.dayPollIntervalS;  // unreachable
}

uint32_t SolarSwitchController::dayTick(uint32_t now) {
  int32_t watts = 0;
  if (power_.readPowerWatts(watts)) {
    const bool on = switch_.isOn();
    const bool next =
        on ? (watts > cfg_.powerOffThresholdW) : (watts >= cfg_.powerOnThresholdW);
    switch_.set(next);
  } else {
    switch_.set(false);  // fail safe: no data, no heating
  }
  history_.onSwitchState(switch_.isOn(), now);
  return cfg_.dayPollIntervalS;
}

uint32_t SolarSwitchController::eveningDecision(uint32_t now) {
  switch_.set(false);
  history_.onSwitchState(false, now);

  float sunshineHours = 0.0f;
  lastForecastValid_ = forecast_.readSunshineHoursTomorrow(sunshineHours);
  lastForecastHours_ = lastForecastValid_ ? sunshineHours : 0.0f;
  lastPastActivationHours_ =
      history_.hoursOnLastDays(timeutil::dayIndex(now), cfg_.boost.pastDaysWindow);

  const NightBoostInputs inputs = {lastForecastValid_, lastForecastHours_,
                                   lastPastActivationHours_};
  boostPlanned_ = shouldNightBoost(cfg_.boost, inputs);
  mode_ = Mode::NightIdle;

  if (boostPlanned_) {
    boostStartEpoch_ = now + timeutil::secondsUntilHour(now, cfg_.boostStartHour);
    return boostStartEpoch_ - now;
  }
  return timeutil::secondsUntilHour(now, cfg_.morningHour);
}

uint32_t SolarSwitchController::nightIdleTick(uint32_t now, uint8_t hour) {
  if (!isNight(hour)) {
    mode_ = Mode::Day;
    return 0;
  }
  if (boostPlanned_) {
    if (now < boostStartEpoch_) return boostStartEpoch_ - now;
    if (now < boostStartEpoch_ + cfg_.boostDurationS) {
      switch_.set(true);
      history_.onSwitchState(true, now);
      mode_ = Mode::NightBoost;
      return boostStartEpoch_ + cfg_.boostDurationS - now;
    }
    boostPlanned_ = false;  // window already passed
  }
  return timeutil::secondsUntilHour(now, cfg_.morningHour);
}

uint32_t SolarSwitchController::endBoost(uint32_t now) {
  switch_.set(false);
  history_.onSwitchState(false, now);
  boostPlanned_ = false;
  mode_ = Mode::NightIdle;
  return timeutil::secondsUntilHour(now, cfg_.morningHour);
}

}  // namespace solar
