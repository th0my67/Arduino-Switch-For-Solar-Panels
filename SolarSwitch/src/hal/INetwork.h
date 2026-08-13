#pragma once

namespace solar {

class INetwork {
 public:
  virtual ~INetwork() {}
  // Blocks until the link is usable (signals progress/errors on the LED).
  virtual bool begin() = 0;
  // Cheap health check + reconnection; call it before making requests.
  virtual bool ensureUp() = 0;
};

}  // namespace solar
