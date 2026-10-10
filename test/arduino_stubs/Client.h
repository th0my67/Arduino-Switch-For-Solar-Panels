// Minimal Arduino Client/Stream/Print stubs for native unit tests.
#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "String.h"
#include "IPAddress.h"

unsigned long millis();
void delay(unsigned long ms);

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* buf, size_t n) {
        size_t total = 0;
        while (n--) total += write(*buf++);
        return total;
    }
    size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); return print(b); }
    size_t print(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); return print(b); }
    size_t print(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); return print(b); }
    size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); return print(b); }
    size_t println(const char* s) { return print(s) + print("\r\n"); }
    size_t println(const String& s) { return println(s.c_str()); }
    size_t println(int v) { return print(v) + print("\r\n"); }
    size_t println(unsigned int v) { return print(v) + print("\r\n"); }
    size_t println(long v) { return print(v) + print("\r\n"); }
    size_t println(unsigned long v) { return print(v) + print("\r\n"); }
    size_t println() { return print("\r\n"); }
};

class Stream : public Print {
protected:
    unsigned long _timeout = 1000;

public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() { return -1; }
    virtual void flush() {}
    void setTimeout(unsigned long timeout) { _timeout = timeout; }
    int timedRead() {
        const unsigned long start = millis();
        do {
            const int c = read();
            if (c >= 0) return c;
        } while ((millis() - start) < _timeout);
        return -1;
    }
    int timedPeek() {
        const unsigned long start = millis();
        do {
            const int c = peek();
            if (c >= 0) return c;
        } while ((millis() - start) < _timeout);
        return -1;
    }
};

class Client : public Stream {
public:
    using Stream::read; // do not hide read() with read(buf, size)
    virtual int connect(IPAddress ip, uint16_t port) = 0;
    virtual int connect(const char* host, uint16_t port) = 0;
    virtual int read(uint8_t* buf, size_t size) {
        size_t count = 0;
        while (count < size && available()) buf[count++] = (uint8_t)read();
        return (int)count;
    }
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
    virtual operator bool() = 0;
    virtual void setTimeout(unsigned long) {}
};

#endif
