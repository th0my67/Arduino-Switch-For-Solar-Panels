// Host-side unit tests for the hardware-independent logic.
// Run with: pio test -e native

#include <string.h>

#include <unity.h>

#include "src/core/ActivationHistory.h"
#include "src/core/JsonScanner.h"
#include "src/core/NightBoostPolicy.h"
#include "src/core/SoftClock.h"
#include "src/core/SolarSwitchController.h"
#include "src/core/TimeUtils.h"
#include "src/services/FroniusClient.h"
#include "src/services/OpenMeteoClient.h"
#include "src/services/SolarEdgeClient.h"

using namespace solar;
using namespace solar::timeutil;

void setUp() {}
void tearDown() {}

// ---- Fakes -----------------------------------------------------------------

struct StringReader : IByteReader {
  const char* s;
  size_t i = 0;
  explicit StringReader(const char* str) : s(str) {}
  int read() override { return s[i] ? (unsigned char)s[i++] : -1; }
};

struct FakeHttp : IHttpClient {
  const char* responseBody = "";
  const char* lastHeaders = nullptr;
  bool fail = false;
  StringReader* reader = nullptr;

  IByteReader* get(const char*, uint16_t, const char*, bool,
                   const char* extraHeaderLines) override {
    lastHeaders = extraHeaderLines;
    if (fail) return nullptr;
    delete reader;
    reader = new StringReader(responseBody);
    return reader;
  }
  void close() override {}
  ~FakeHttp() { delete reader; }
};

struct FakeClock : IClock {
  uint32_t t = 0;
  bool isValid() const override { return true; }
  uint32_t now() override { return t; }
  void sync(uint32_t e) override { t = e; }
};

struct FakeSwitch : ISwitchOutput {
  bool on = false;
  void set(bool v) override { on = v; }
  bool isOn() const override { return on; }
};

struct FakePower : IPowerReader {
  bool ok = true;
  int32_t watts = 0;
  bool readPowerWatts(int32_t& out) override {
    out = watts;
    return ok;
  }
};

struct FakeForecast : ISunshineForecast {
  bool ok = true;
  float hours = 0.0f;
  int calls = 0;
  bool readSunshineHoursTomorrow(float& out) override {
    calls++;
    out = hours;
    return ok;
  }
};

// ---- TimeUtils -------------------------------------------------------------

void test_civil_to_epoch() {
  TEST_ASSERT_EQUAL_UINT32(0UL, civilToEpoch(1970, 1, 1, 0, 0, 0));
  // 2024-04-08 13:45:30 = 1712583930
  TEST_ASSERT_EQUAL_UINT32(1712583930UL, civilToEpoch(2024, 4, 8, 13, 45, 30));
}

void test_parse_iso_local() {
  uint32_t epoch = 0;
  TEST_ASSERT_TRUE(parseIsoLocal("2024-04-08T13:45:30+02:00", epoch));
  TEST_ASSERT_EQUAL_UINT32(1712583930UL, epoch);
  TEST_ASSERT_FALSE(parseIsoLocal("garbage", epoch));
  TEST_ASSERT_FALSE(parseIsoLocal("2024-13-08T13:45:30", epoch));
}

void test_seconds_until_hour() {
  const uint32_t at1345 = civilToEpoch(2024, 4, 8, 13, 45, 30);
  TEST_ASSERT_EQUAL_UINT32(26070UL, secondsUntilHour(at1345, 21));
  TEST_ASSERT_EQUAL_UINT32(58470UL, secondsUntilHour(at1345, 6));  // next day
  const uint32_t at2100 = civilToEpoch(2024, 4, 8, 21, 0, 0);
  TEST_ASSERT_EQUAL_UINT32(5 * 3600UL, secondsUntilHour(at2100, 2));
  TEST_ASSERT_EQUAL_UINT32(kSecondsPerDay, secondsUntilHour(at2100, 21));
}

void test_day_and_hour() {
  const uint32_t e = civilToEpoch(2024, 4, 8, 23, 59, 59);
  TEST_ASSERT_EQUAL_INT(23, hourOfDay(e));
  TEST_ASSERT_EQUAL_UINT32(dayIndex(e) + 1, dayIndex(e + 1));
}

// ---- ActivationHistory -----------------------------------------------------

void test_history_accumulates_on_time() {
  ActivationHistory h;
  const uint32_t day0 = civilToEpoch(2024, 4, 8, 0, 0, 0);
  h.begin(day0 + 10 * 3600);
  h.onSwitchState(true, day0 + 10 * 3600);   // ON at 10:00
  h.onSwitchState(true, day0 + 11 * 3600);   // still ON (poll)
  h.onSwitchState(false, day0 + 12 * 3600 + 1800);  // OFF at 12:30
  h.onSwitchState(false, day0 + 14 * 3600);
  TEST_ASSERT_EQUAL_UINT32(9000UL, h.secondsOnForDay(dayIndex(day0)));  // 2.5h
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, h.hoursOnLastDays(dayIndex(day0), 2));
}

void test_history_splits_across_midnight() {
  ActivationHistory h;
  const uint32_t day0 = civilToEpoch(2024, 4, 8, 0, 0, 0);
  h.begin(day0 + 23 * 3600);
  h.onSwitchState(true, day0 + 23 * 3600);                  // ON at 23:00
  h.onSwitchState(false, day0 + kSecondsPerDay + 3600);     // OFF at 01:00
  TEST_ASSERT_EQUAL_UINT32(3600UL, h.secondsOnForDay(dayIndex(day0)));
  TEST_ASSERT_EQUAL_UINT32(3600UL, h.secondsOnForDay(dayIndex(day0) + 1));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f,
                           h.hoursOnLastDays(dayIndex(day0) + 1, 2));
  // A 2-day window anchored two days later only sees the second hour.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f,
                           h.hoursOnLastDays(dayIndex(day0) + 2, 2));
}

// ---- NightBoostPolicy ------------------------------------------------------

void test_night_boost_policy() {
  const NightBoostConfig cfg = {4.0f, 6.0f, 2, false};
  // Little sun forecast + little recent activation -> boost.
  TEST_ASSERT_TRUE(shouldNightBoost(cfg, {true, 2.0f, 1.0f}));
  // Sunny tomorrow -> no boost.
  TEST_ASSERT_FALSE(shouldNightBoost(cfg, {true, 8.0f, 1.0f}));
  // Enough activation already -> no boost even if tomorrow is dark.
  TEST_ASSERT_FALSE(shouldNightBoost(cfg, {true, 2.0f, 7.0f}));
  // Forecast unavailable -> configurable fallback.
  TEST_ASSERT_FALSE(shouldNightBoost(cfg, {false, 0.0f, 1.0f}));
  const NightBoostConfig cfgFallback = {4.0f, 6.0f, 2, true};
  TEST_ASSERT_TRUE(shouldNightBoost(cfgFallback, {false, 0.0f, 1.0f}));
  TEST_ASSERT_FALSE(shouldNightBoost(cfgFallback, {false, 0.0f, 7.0f}));
}

// ---- JsonScanner -----------------------------------------------------------

void test_json_fronius_power() {
  StringReader in(
      "{\"Body\":{\"Data\":{\"DAY_ENERGY\":{\"Unit\":\"Wh\",\"Value\":8000},"
      "\"PAC\":{\"Unit\":\"W\",\"Value\":5321},"
      "\"TOTAL_ENERGY\":{\"Unit\":\"Wh\",\"Value\":100}}},"
      "\"Head\":{\"Timestamp\":\"2024-04-08T13:45:30+02:00\"}}");
  JsonScanner json(in);
  float value = 0;
  TEST_ASSERT_TRUE(json.findKey("PAC"));
  TEST_ASSERT_TRUE(json.findKey("Value"));
  TEST_ASSERT_TRUE(json.readFloat(value));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5321.0f, value);
  char buf[26];
  TEST_ASSERT_TRUE(json.findKey("Timestamp"));
  TEST_ASSERT_TRUE(json.readString(buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("2024-04-08T13:45:30+02:00", buf);
}

void test_json_open_meteo_daily_units_trap() {
  // "daily_units" appears before "daily" and also contains a
  // "sunshine_duration" key whose value is a string — the scanner must not
  // be fooled by either.
  StringReader in(
      "{\"latitude\":46.52,\"utc_offset_seconds\":7200,"
      "\"daily_units\":{\"time\":\"iso8601\",\"sunshine_duration\":\"s\"},"
      "\"daily\":{\"time\":[\"2026-08-13\",\"2026-08-14\"],"
      "\"sunshine_duration\":[35208.00,12600.00]}}");
  JsonScanner json(in);
  float values[2];
  uint8_t count = 0;
  TEST_ASSERT_TRUE(json.findKey("daily"));
  TEST_ASSERT_TRUE(json.findKey("sunshine_duration"));
  TEST_ASSERT_TRUE(json.readFloatArray(values, 2, count));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 35208.0f, values[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12600.0f, values[1]);
}

void test_json_null_in_array() {
  StringReader in("\"x\":[null, 3.5]");
  JsonScanner json(in);
  float values[2];
  uint8_t count = 0;
  TEST_ASSERT_TRUE(json.findKey("x"));
  TEST_ASSERT_TRUE(json.readFloatArray(values, 2, count));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, values[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.5f, values[1]);
}

// ---- Service adapters ------------------------------------------------------

void test_fronius_client() {
  FakeHttp http;
  http.responseBody =
      "{\"Body\":{\"Data\":{\"PAC\":{\"Unit\":\"W\",\"Value\":4863}}},"
      "\"Head\":{\"Timestamp\":\"2024-04-08T13:45:30+02:00\"}}";
  const FroniusConfig cfg = {"host", 80, "/path"};
  FroniusClient client(http, cfg);
  int32_t watts = 0;
  TEST_ASSERT_TRUE(client.readPowerWatts(watts));
  TEST_ASSERT_EQUAL_INT32(4863, watts);
  uint32_t epoch = 0;
  TEST_ASSERT_TRUE(client.readLocalEpoch(epoch));
  TEST_ASSERT_EQUAL_UINT32(1712583930UL, epoch);
}

void test_solaredge_client() {
  FakeHttp http;
  http.responseBody =
      "{\"updateRefreshRate\":3,\"grid\":{\"power\":120.0},"
      "\"pv\":{\"active\":true,\"power\":3.217},\"load\":{\"power\":500}}";
  // "grid" exposes a "power" key before "pv": the scanner must skip it and
  // only read the one inside "pv".
  const SolarEdgeConfig cfg = {"host", 443, "/path", "X-API-KEY: k\r\n", true,
                               1000.0f};
  SolarEdgeClient client(http, cfg);
  int32_t watts = 0;
  TEST_ASSERT_TRUE(client.readPowerWatts(watts));
  TEST_ASSERT_EQUAL_INT32(3217, watts);
  TEST_ASSERT_EQUAL_STRING("X-API-KEY: k\r\n", http.lastHeaders);
}

void test_open_meteo_client() {
  FakeHttp http;
  http.responseBody =
      "{\"daily_units\":{\"sunshine_duration\":\"s\"},"
      "\"daily\":{\"time\":[\"a\",\"b\"],\"sunshine_duration\":[36000,7200]}}";
  const WeatherConfig cfg = {"host", 443, "/path", true};
  OpenMeteoClient client(http, cfg);
  float hours = 0;
  TEST_ASSERT_TRUE(client.readSunshineHoursTomorrow(hours));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, hours);  // 7200 s -> 2 h

  http.fail = true;
  TEST_ASSERT_FALSE(client.readSunshineHoursTomorrow(hours));
}

// ---- Controller ------------------------------------------------------------

static ControllerConfig makeControllerConfig() {
  ControllerConfig cfg;
  cfg.dayPollIntervalS = 90;
  cfg.powerOnThresholdW = 5000;
  cfg.powerOffThresholdW = 4500;
  cfg.eveningHour = 21;
  cfg.morningHour = 6;
  cfg.boostStartHour = 2;
  cfg.boostDurationS = 3 * 3600;
  cfg.boost = {4.0f, 6.0f, 2, false};
  return cfg;
}

void test_controller_day_hysteresis() {
  FakeClock clock;
  FakeSwitch sw;
  FakePower power;
  FakeForecast forecast;
  ActivationHistory history;
  SolarSwitchController c(makeControllerConfig(), clock, sw, power, forecast,
                          history);

  clock.t = civilToEpoch(2024, 4, 8, 12, 0, 0);
  history.begin(clock.t);

  power.watts = 6000;
  TEST_ASSERT_EQUAL_UINT32(90UL, c.tick());
  TEST_ASSERT_TRUE(sw.isOn());

  power.watts = 4800;  // between thresholds: stays ON (hysteresis)
  clock.t += 90;
  c.tick();
  TEST_ASSERT_TRUE(sw.isOn());

  power.watts = 4400;  // below OFF threshold
  clock.t += 90;
  c.tick();
  TEST_ASSERT_FALSE(sw.isOn());

  power.watts = 4800;  // between thresholds: stays OFF
  clock.t += 90;
  c.tick();
  TEST_ASSERT_FALSE(sw.isOn());

  power.ok = false;  // inverter unreachable -> fail safe OFF
  power.watts = 9000;
  clock.t += 90;
  c.tick();
  TEST_ASSERT_FALSE(sw.isOn());
}

void test_controller_full_night_cycle_with_boost() {
  FakeClock clock;
  FakeSwitch sw;
  FakePower power;
  FakeForecast forecast;
  ActivationHistory history;
  SolarSwitchController c(makeControllerConfig(), clock, sw, power, forecast,
                          history);

  // Cloudy day: little production, switch stays off.
  clock.t = civilToEpoch(2024, 4, 8, 12, 0, 0);
  history.begin(clock.t);
  power.watts = 800;
  c.tick();
  TEST_ASSERT_FALSE(sw.isOn());
  TEST_ASSERT_EQUAL_INT(0, (int)c.mode());  // Day

  // 21:00 — evening decision: dark forecast + no recent activation -> boost.
  clock.t = civilToEpoch(2024, 4, 8, 21, 0, 0);
  forecast.hours = 2.0f;
  uint32_t sleepS = c.tick();
  TEST_ASSERT_EQUAL_INT(1, forecast.calls);
  TEST_ASSERT_TRUE(c.boostPlanned());
  TEST_ASSERT_EQUAL_UINT32(5 * 3600UL, sleepS);  // until 02:00
  TEST_ASSERT_FALSE(sw.isOn());

  // 02:00 — boost starts.
  clock.t += sleepS;
  sleepS = c.tick();
  TEST_ASSERT_TRUE(sw.isOn());
  TEST_ASSERT_EQUAL_UINT32(3 * 3600UL, sleepS);

  // 05:00 — boost ends.
  clock.t += sleepS;
  sleepS = c.tick();
  TEST_ASSERT_FALSE(sw.isOn());
  TEST_ASSERT_EQUAL_UINT32(3600UL, sleepS);  // until 06:00

  // 06:00 — back to day mode.
  clock.t += sleepS;
  TEST_ASSERT_EQUAL_UINT32(0UL, c.tick());
  power.watts = 6000;
  c.tick();
  TEST_ASSERT_TRUE(sw.isOn());

  // The 3h boost is now recorded as activation for the new day.
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f,
                           history.hoursOnLastDays(dayIndex(clock.t), 1));
}

void test_controller_no_boost_when_sunny_or_active() {
  FakeClock clock;
  FakeSwitch sw;
  FakePower power;
  FakeForecast forecast;
  ActivationHistory history;
  SolarSwitchController c(makeControllerConfig(), clock, sw, power, forecast,
                          history);

  // Sunny day: switch ON from 10:00 until the evening decision (11h total).
  clock.t = civilToEpoch(2024, 4, 8, 10, 0, 0);
  history.begin(clock.t);
  power.watts = 6000;
  c.tick();
  TEST_ASSERT_TRUE(sw.isOn());
  clock.t = civilToEpoch(2024, 4, 8, 17, 0, 0);
  c.tick();

  // Evening: even with a dark forecast, 11h > 6h max -> no boost.
  clock.t = civilToEpoch(2024, 4, 8, 21, 0, 0);
  forecast.hours = 1.0f;
  const uint32_t sleepS = c.tick();
  TEST_ASSERT_FALSE(c.boostPlanned());
  TEST_ASSERT_EQUAL_UINT32(9 * 3600UL, sleepS);  // straight to 06:00
  TEST_ASSERT_FALSE(sw.isOn());
}

void test_controller_night_reboot_stays_off() {
  FakeClock clock;
  FakeSwitch sw;
  FakePower power;
  FakeForecast forecast;
  ActivationHistory history;
  SolarSwitchController c(makeControllerConfig(), clock, sw, power, forecast,
                          history);

  // Booting at 03:00: no evening decision happened, no boost, wait for 06:00.
  clock.t = civilToEpoch(2024, 4, 9, 3, 0, 0);
  history.begin(clock.t);
  const uint32_t sleepS = c.tick();
  TEST_ASSERT_FALSE(sw.isOn());
  TEST_ASSERT_EQUAL_INT(0, forecast.calls);
  TEST_ASSERT_EQUAL_UINT32(3 * 3600UL, sleepS);
}

// ---- SoftClock -------------------------------------------------------------

static uint32_t fakeMillisValue = 0;
static uint32_t fakeMillis() { return fakeMillisValue; }

void test_soft_clock() {
  SoftClock clock(fakeMillis);
  TEST_ASSERT_FALSE(clock.isValid());
  fakeMillisValue = 1000;
  clock.sync(5000);
  TEST_ASSERT_TRUE(clock.isValid());
  TEST_ASSERT_EQUAL_UINT32(5000UL, clock.now());
  fakeMillisValue += 2500;  // 2.5 s pass
  TEST_ASSERT_EQUAL_UINT32(5002UL, clock.now());
  fakeMillisValue += 500;  // remainder handling: total 3 s
  TEST_ASSERT_EQUAL_UINT32(5003UL, clock.now());
  clock.advance(60000);  // a 60 s sleep during which millis() was frozen
  TEST_ASSERT_EQUAL_UINT32(5063UL, clock.now());
}

// ----------------------------------------------------------------------------

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_civil_to_epoch);
  RUN_TEST(test_parse_iso_local);
  RUN_TEST(test_seconds_until_hour);
  RUN_TEST(test_day_and_hour);
  RUN_TEST(test_history_accumulates_on_time);
  RUN_TEST(test_history_splits_across_midnight);
  RUN_TEST(test_night_boost_policy);
  RUN_TEST(test_json_fronius_power);
  RUN_TEST(test_json_open_meteo_daily_units_trap);
  RUN_TEST(test_json_null_in_array);
  RUN_TEST(test_fronius_client);
  RUN_TEST(test_solaredge_client);
  RUN_TEST(test_open_meteo_client);
  RUN_TEST(test_controller_day_hysteresis);
  RUN_TEST(test_controller_full_night_cycle_with_boost);
  RUN_TEST(test_controller_no_boost_when_sunny_or_active);
  RUN_TEST(test_controller_night_reboot_stays_off);
  RUN_TEST(test_soft_clock);
  return UNITY_END();
}
