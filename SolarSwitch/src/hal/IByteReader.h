#pragma once
#include <stdint.h>

namespace solar {

// Forward-only byte stream. Lets the parsing code consume large HTTP bodies
// without ever holding them in RAM, and without depending on Arduino types.
class IByteReader {
 public:
  virtual ~IByteReader() {}
  // Returns the next byte (0..255), or -1 when the stream ended or timed out.
  virtual int read() = 0;
};

}  // namespace solar
