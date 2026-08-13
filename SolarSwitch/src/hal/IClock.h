#pragma once
#include <stdint.h>

namespace solar {

// Local wall-clock time expressed as seconds since 1970-01-01 00:00 *local*
// ("local epoch"). Helpers in core/TimeUtils.h derive day and hour from it.
class IClock {
 public:
  virtual ~IClock() {}
  virtual bool isValid() const = 0;
  virtual uint32_t now() = 0;
  virtual void sync(uint32_t localEpoch) = 0;
};

}  // namespace solar
