// Solar Panels / Inverter Arduino Switch — composition root.
//
// All decision logic lives in src/core (hardware-independent, unit-tested on
// the host); src/services adapts the Fronius and weather HTTP APIs; this file
// only selects the platform adapters and wires everything together.
// To port to another board, provide implementations of the interfaces in
// src/hal and add a branch below — nothing else changes.

#include "src/config/PlatformSelect.h"
#include "src/config/AppConfig.h"

#include "src/core/ActivationHistory.h"
#include "src/core/SoftClock.h"
#include "src/core/SolarSwitchController.h"
#include "src/platform/HttpOverClient.h"
#include "src/platform/SamdPlatform.h"
#include "src/services/OpenMeteoClient.h"

#if SOLAR_INVERTER_SOLAREDGE
#include "src/services/SolarEdgeClient.h"
#else
#include "src/services/FroniusClient.h"
#endif

#if SOLAR_PLATFORM_MKR_WIFI
#include "src/config/Secrets.h"
#include "src/platform/WiFiPlatform.h"
#else
#include "src/platform/EthernetPlatform.h"
#endif

// Set to 1 to get logs on the USB serial port. Note: deep sleep drops the
// USB connection, so keep this for bench debugging only.
#ifndef SOLAR_DEBUG
#define SOLAR_DEBUG 0
#endif
#if SOLAR_DEBUG
#define LOG(x) Serial.println(x)
#else
#define LOG(x)
#endif

using namespace solar;

static uint32_t millisNow() { return (uint32_t)millis(); }

// ---- Platform wiring -------------------------------------------------------
BuiltinStatusLed led;
GpioSwitch switchOutput(config::kSwitchPin);
SoftClock softClock(millisNow);
SamdSleeper sleeper(softClock);

#if SOLAR_PLATFORM_MKR_WIFI
WiFiNetwork network(WIFI_SSID, WIFI_PASSWORD, led);
WiFiClient plainClient;
WiFiSSLClient tlsClient;
HttpOverClient http(plainClient, &tlsClient);
#else
EthernetNetwork network(config::kEthernetMac, led);
EthernetClient plainClient;
HttpOverClient http(plainClient, nullptr);  // no TLS on the ETH shield
#endif

// ---- Services --------------------------------------------------------------
// The inverter vendor is a plug-in: any IPowerReader works here.
#if SOLAR_INVERTER_SOLAREDGE
SolarEdgeConfig inverterConfig = {
    config::kInverterHost,        config::kInverterPort,
    config::kInverterPath,        config::kInverterHeaders,
    config::kInverterUseTls,      config::kSolarEdgePowerScaleToWatts};
SolarEdgeClient inverter(http, inverterConfig);
#else
FroniusConfig inverterConfig = {config::kInverterHost, config::kInverterPort,
                                config::kInverterPath};
FroniusClient inverter(http, inverterConfig);
#endif

// Wall-clock reference: Fronius exposes its own (local, DST-aware)
// timestamp; SolarEdge does not, so WiFi builds fall back to NTP.
#if SOLAR_INVERTER_FRONIUS
ITimeSource& timeSource = inverter;
#elif SOLAR_PLATFORM_MKR_WIFI
NinaTimeSource timeSource(config::kUtcOffsetSeconds);
#else
#error \
    "SolarEdge + Ethernet has no time source: implement ITimeSource (e.g. NTP over EthernetUDP) and wire it here."
#endif

WeatherConfig weatherConfig = {config::kWeatherHost, config::kWeatherPort,
                               config::kWeatherPath, config::kWeatherUseTls};
OpenMeteoClient weather(http, weatherConfig);

// ---- Core logic -------------------------------------------------------------
ActivationHistory history;

ControllerConfig controllerConfig = {
    config::kDayPollIntervalS,
    config::kPowerOnThresholdW,
    config::kPowerOffThresholdW,
    config::kEveningHour,
    config::kMorningHour,
    config::kBoostStartHour,
    config::kBoostDurationS,
    {config::kMinForecastSunshineHours, config::kMaxPastActivationHours,
     config::kPastDaysWindow, config::kBoostWhenForecastUnavailable},
};
SolarSwitchController controller(controllerConfig, softClock, switchOutput,
                                 inverter, weather, history);

uint32_t lastClockSyncAttempt = 0;

static void syncClock() {
  uint32_t epoch = 0;
  if (timeSource.readLocalEpoch(epoch)) {
    softClock.sync(epoch);
    LOG("Clock synced");
  }
}

void setup() {
  led.begin();
  switchOutput.begin();
#if SOLAR_DEBUG
  Serial.begin(115200);
#endif
  network.begin();

  // Wall time comes from the inverter's own timestamp; without it the
  // day/night schedule cannot run, so block until the first sync succeeds.
  while (!softClock.isValid()) {
    syncClock();
    if (!softClock.isValid()) {
      led.signal(LedEvent::HostUnreachable);
      sleeper.lightSleep(60000);
    }
  }
  history.begin(softClock.now());
}

void loop() {
  network.ensureUp();

  const uint32_t now = softClock.now();
  if (now - lastClockSyncAttempt >= config::kClockResyncIntervalS) {
    lastClockSyncAttempt = now;
    syncClock();  // best effort; the SoftClock keeps running on failure
  }

  const uint32_t sleepSeconds = controller.tick();

#if SOLAR_DEBUG
  Serial.print("mode=");
  Serial.print((int)controller.mode());
  Serial.print(" switch=");
  Serial.print(switchOutput.isOn());
  Serial.print(" boostPlanned=");
  Serial.print(controller.boostPlanned());
  Serial.print(" forecastH=");
  Serial.print(controller.lastForecastHours());
  Serial.print(" pastH=");
  Serial.print(controller.lastPastActivationHours());
  Serial.print(" sleepS=");
  Serial.println(sleepSeconds);
#endif

  if (sleepSeconds == 0) return;  // state transition: tick again immediately

  if (sleepSeconds >= config::kDeepSleepThresholdS) {
    led.set(false);
    sleeper.deepSleep(sleepSeconds * 1000UL);
  } else {
    led.signal(LedEvent::Heartbeat);
    sleeper.lightSleep(sleepSeconds * 1000UL);
  }
}
