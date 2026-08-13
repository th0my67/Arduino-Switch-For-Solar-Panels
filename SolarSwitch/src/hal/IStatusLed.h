#pragma once
#include <stdint.h>

namespace solar {

enum class LedEvent : uint8_t {
  LinkError,        // no network link (cable unplugged / WiFi unreachable)
  DhcpError,        // link up but no IP address
  HostUnreachable,  // network up but the inverter does not answer
  Heartbeat,        // short blip before going back to sleep
};

class IStatusLed {
 public:
  virtual ~IStatusLed() {}
  virtual void set(bool on) = 0;
  // Plays a short blocking blink pattern identifying the event.
  virtual void signal(LedEvent event) = 0;
};

}  // namespace solar
