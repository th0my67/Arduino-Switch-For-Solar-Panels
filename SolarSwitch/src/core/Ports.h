#pragma once
#include <stdint.h>

namespace solar {

// "Ports" in the hexagonal-architecture sense: what the core logic needs
// from the outside world. Adapters in services/ and platform/ implement
// them; tests implement them with fakes.
//
// Swapping the inverter vendor means providing another IPowerReader (and,
// if the vendor's API carries a usable timestamp, an ITimeSource) — nothing
// in core/ changes.

class IPowerReader {
 public:
  virtual ~IPowerReader() {}
  // Current production of the inverter, in watts. False when unavailable.
  virtual bool readPowerWatts(int32_t& watts) = 0;
};

class ITimeSource {
 public:
  virtual ~ITimeSource() {}
  // Current local wall-clock time as a "local epoch" (see hal/IClock.h).
  virtual bool readLocalEpoch(uint32_t& epoch) = 0;
};

class ISunshineForecast {
 public:
  virtual ~ISunshineForecast() {}
  // Forecast sunshine hours for tomorrow (local calendar day).
  virtual bool readSunshineHoursTomorrow(float& hours) = 0;
};

}  // namespace solar
