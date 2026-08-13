#pragma once
#include <Arduino.h>
#include <ArduinoLowPower.h>

#include "../core/SoftClock.h"
#include "../hal/ISleeper.h"
#include "../hal/IStatusLed.h"
#include "../hal/ISwitchOutput.h"

namespace solar {

class GpioSwitch : public ISwitchOutput {
 public:
  explicit GpioSwitch(uint8_t pin) : pin_(pin) {}
  void begin() {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
  }
  void set(bool on) override {
    on_ = on;
    digitalWrite(pin_, on ? HIGH : LOW);
  }
  bool isOn() const override { return on_; }

 private:
  uint8_t pin_;
  bool on_ = false;
};

// Keeps the legacy blink patterns so existing debugging habits still work.
class BuiltinStatusLed : public IStatusLed {
 public:
  void begin() { pinMode(LED_BUILTIN, OUTPUT); }
  void set(bool on) override { digitalWrite(LED_BUILTIN, on ? HIGH : LOW); }

  void signal(LedEvent event) override {
    switch (event) {
      case LedEvent::LinkError:  // ON 4s; OFF 0.5s; ON 1s; OFF 0.5s
        blink(4000);
        blink(1000);
        break;
      case LedEvent::DhcpError:  // ON 1s; OFF 0.5s; ON 1s; OFF 0.5s
        blink(1000);
        blink(1000);
        break;
      case LedEvent::HostUnreachable:  // ON 4s; OFF 4s
        blink(4000, 4000);
        break;
      case LedEvent::Heartbeat:  // short blip
        blink(100, 100);
        break;
    }
  }

 private:
  void blink(uint32_t onMs, uint32_t offMs = 500) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(onMs);
    digitalWrite(LED_BUILTIN, LOW);
    delay(offMs);
  }
};

// SAMD21 sleep. millis() freezes in standby, so slept time is pushed back
// into the SoftClock to keep wall time consistent.
class SamdSleeper : public ISleeper {
 public:
  explicit SamdSleeper(SoftClock& clock) : clock_(clock) {}

  void lightSleep(uint32_t ms) override {
    LowPower.sleep((int)ms);
    clock_.advance(ms);
  }

  void deepSleep(uint32_t ms) override {
    LowPower.deepSleep((int)ms);
    clock_.advance(ms);
  }

 private:
  SoftClock& clock_;
};

}  // namespace solar
