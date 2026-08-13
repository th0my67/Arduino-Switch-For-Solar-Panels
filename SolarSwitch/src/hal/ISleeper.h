#pragma once
#include <stdint.h>

namespace solar {

class ISleeper {
 public:
  virtual ~ISleeper() {}
  virtual void lightSleep(uint32_t ms) = 0;  // fast wake, used between polls
  virtual void deepSleep(uint32_t ms) = 0;   // lowest power; USB serial is lost
};

}  // namespace solar
