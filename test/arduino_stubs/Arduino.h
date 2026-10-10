// Minimal Arduino core stub for native unit tests.
#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include "String.h"
#include "IPAddress.h"
#include "Client.h"
#include "Udp.h"

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define LED_BUILTIN 6

typedef uint8_t byte;
typedef bool boolean;

#define isHexadecimalDigit(c) (isxdigit((int)(unsigned char)(c)))

inline bool isAlphaNumeric(int c) { return isalnum(c) != 0; }
inline bool isAlpha(int c) { return isalpha(c) != 0; }
inline bool isAscii(int c) { return c >= 0 && c <= 127; }
inline bool isWhitespace(int c) { return isblank(c) != 0; }
inline bool isControl(int c) { return iscntrl(c) != 0; }
inline bool isDigit(int c) { return isdigit(c) != 0; }
inline bool isGraph(int c) { return isgraph(c) != 0; }
inline bool isLowerCase(int c) { return islower(c) != 0; }
inline bool isPunct(int c) { return ispunct(c) != 0; }
inline bool isSpace(int c) { return isspace(c) != 0; }
inline bool isUpperCase(int c) { return isupper(c) != 0; }

inline long random(long maxval) { return maxval > 0 ? (long)(std::rand() % maxval) : 0; }
inline long random(long minval, long maxval) {
    if (maxval <= minval) return minval;
    return minval + (long)(std::rand() % (maxval - minval));
}

using std::min;
using std::max;

namespace stubs {
inline unsigned long g_millis = 0;
inline int g_pinMode[32];
inline int g_pinState[32];

inline void resetStubs() {
    g_millis = 0;
    for (int i = 0; i < 32; ++i) { g_pinMode[i] = -1; g_pinState[i] = -1; }
}
inline void advanceMillis(unsigned long ms) { g_millis += ms; }
inline int pinState(int pin) { return (pin >= 0 && pin < 32) ? g_pinState[pin] : -1; }
inline int pinModeSet(int pin) { return (pin >= 0 && pin < 32) ? g_pinMode[pin] : -1; }
} // namespace stubs

inline unsigned long millis() { return stubs::g_millis; }
inline void delay(unsigned long ms) { stubs::g_millis += ms; }

inline void pinMode(uint8_t pin, uint8_t mode) { if (pin < 32) stubs::g_pinMode[pin] = mode; }
inline void digitalWrite(uint8_t pin, uint8_t value) { if (pin < 32) stubs::g_pinState[pin] = value; }
inline int digitalRead(uint8_t pin) { return (pin < 32) ? stubs::g_pinState[pin] : 0; }

#endif
