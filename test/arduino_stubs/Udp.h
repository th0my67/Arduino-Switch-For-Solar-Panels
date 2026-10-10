// Minimal Arduino UDP stub for native unit tests.
#ifndef TEST_UDP_H
#define TEST_UDP_H

#include <cstddef>
#include <cstdint>
#include "Client.h"

class UDP : public Stream {
public:
    virtual uint8_t begin(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual int beginPacket(const char* host, uint16_t port) = 0;
    virtual int beginPacket(IPAddress ip, uint16_t port) = 0;
    virtual int endPacket() = 0;
    virtual size_t write(const uint8_t* buf, size_t size) = 0;
    virtual size_t write(uint8_t b) = 0;
    virtual int parsePacket() = 0;
    virtual int read(uint8_t* buf, size_t size) = 0;
    virtual int read() = 0;
    virtual int available() = 0;
    virtual int peek() = 0;
    virtual void flush() {}
    virtual IPAddress remoteIP() = 0;
    virtual uint16_t remotePort() = 0;
};

#endif
