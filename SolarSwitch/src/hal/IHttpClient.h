#pragma once
#include <stdint.h>

#include "IByteReader.h"

namespace solar {

class IHttpClient {
 public:
  virtual ~IHttpClient() {}
  // Performs a GET request and consumes the status line + headers.
  // extraHeaderLines: nullptr, or zero or more complete header lines each
  // terminated by "\r\n" (e.g. "X-API-KEY: abc\r\nAccept: application/json\r\n").
  // Returns a reader positioned at the start of the body, or nullptr on
  // failure (connection error, non-200 status, TLS not supported...).
  // Only one request can be open at a time; call close() when done.
  virtual IByteReader* get(const char* host, uint16_t port,
                           const char* pathAndQuery, bool tls,
                           const char* extraHeaderLines) = 0;
  virtual void close() = 0;
};

}  // namespace solar
