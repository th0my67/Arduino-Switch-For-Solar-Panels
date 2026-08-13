#pragma once
#include <stdint.h>

namespace solar {
namespace timeutil {

constexpr uint32_t kSecondsPerDay = 86400UL;

inline uint32_t dayIndex(uint32_t epoch) { return epoch / kSecondsPerDay; }

inline uint8_t hourOfDay(uint32_t epoch) {
  return (uint8_t)((epoch % kSecondsPerDay) / 3600UL);
}

inline uint32_t startOfDay(uint32_t epoch) {
  return epoch - (epoch % kSecondsPerDay);
}

// Seconds until the next occurrence of `hour` o'clock (strictly in the
// future: asking for the current hour returns ~24h).
inline uint32_t secondsUntilHour(uint32_t now, uint8_t hour) {
  uint32_t target = startOfDay(now) + hour * 3600UL;
  if (target <= now) target += kSecondsPerDay;
  return target - now;
}

// Howard Hinnant's days-from-civil algorithm (proleptic Gregorian calendar).
inline int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

inline uint32_t civilToEpoch(int32_t year, uint32_t month, uint32_t day,
                             uint32_t hour, uint32_t minute, uint32_t second) {
  return (uint32_t)daysFromCivil(year, month, day) * kSecondsPerDay +
         hour * 3600UL + minute * 60UL + second;
}

// Parses the date/time part of an ISO-8601 string such as
// "2024-04-08T13:45:30+02:00". The timezone suffix is deliberately ignored:
// the wall-clock part is taken as-is, matching the "local epoch" convention.
inline bool parseIsoLocal(const char* s, uint32_t& epochOut) {
  struct Digits {
    static bool read(const char* p, uint8_t n, uint32_t& v) {
      v = 0;
      for (uint8_t i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return false;
        v = v * 10 + (uint32_t)(p[i] - '0');
      }
      return true;
    }
  };
  uint32_t y, mo, d, h, mi, sec;
  if (!Digits::read(s, 4, y) || s[4] != '-' || !Digits::read(s + 5, 2, mo) ||
      s[7] != '-' || !Digits::read(s + 8, 2, d) ||
      (s[10] != 'T' && s[10] != ' ') || !Digits::read(s + 11, 2, h) ||
      s[13] != ':' || !Digits::read(s + 14, 2, mi) || s[16] != ':' ||
      !Digits::read(s + 17, 2, sec)) {
    return false;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60) {
    return false;
  }
  epochOut = civilToEpoch((int32_t)y, mo, d, h, mi, sec);
  return true;
}

}  // namespace timeutil
}  // namespace solar
