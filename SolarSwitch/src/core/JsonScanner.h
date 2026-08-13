#pragma once
#include <stdint.h>

#include "../hal/IByteReader.h"

namespace solar {

// Minimal forward-only extractor for JSON payloads. Works on a byte stream,
// so multi-kilobyte documents never need to fit in RAM. It is not a JSON
// validator: it finds keys and pulls simple values out, which is all this
// application needs.
class JsonScanner {
 public:
  explicit JsonScanner(IByteReader& in) : in_(in) {}

  // Advances the stream until the exact quoted key ("name") is consumed.
  // "daily" will NOT match inside "daily_units": the closing quote is part
  // of the pattern.
  bool findKey(const char* name);

  // Skips ':' and whitespace, then parses one number.
  bool readFloat(float& out);

  // Reads the next quoted string value into buf (always NUL-terminated;
  // extra characters are dropped but the stream stays consistent).
  bool readString(char* buf, uint8_t bufLen);

  // Reads a numeric array like [1, 2.5, null]; null becomes 0. Extra
  // elements beyond maxCount are drained. count receives values stored.
  bool readFloatArray(float* out, uint8_t maxCount, uint8_t& count);

 private:
  bool parseNumber(float& out, int& terminator);
  int skipUntil(char target);
  IByteReader& in_;
};

}  // namespace solar
