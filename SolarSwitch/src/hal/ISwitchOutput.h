#pragma once

namespace solar {

// The relay/contactor driven by the application (e.g. the boiler switch).
class ISwitchOutput {
 public:
  virtual ~ISwitchOutput() {}
  virtual void set(bool on) = 0;
  virtual bool isOn() const = 0;
};

}  // namespace solar
