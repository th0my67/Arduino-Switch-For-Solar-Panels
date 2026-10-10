#include <Arduino.h>
#include <ArduinoLowPower.h>
#include <RTCZero.h>

#include "config.h"
#include "hal/implementations/wifi_network.h"
#include "hal/implementations/arduino_http_client_adapter.h"
#if INVERTER_BRAND == INVERTER_BRAND_SOLAREDGE
#include "api/solaredge_api_v2.h"
#else
#include "api/fronius_solar_api_v1.h"
#endif
#include "services/power_monitor.h"
#include "services/activation_tracker.h"
#include "services/backup_charge.h"
#include "api/open_meteo_forecast.h"
#include "switch_controller.h"
#include "time_manager.h"
#include "utils/logging.h"

#if DEBUG
// Debug LED patterns (see README):
//   ON 4s / OFF 4s : no connection to the host
//   ON 1s / OFF 4s : waiting for the next request
//   ON 0.1s / OFF 0.1s : waiting for the response
static void blinkLed(uint32_t onMs, uint32_t offMs) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(onMs);
    digitalWrite(LED_BUILTIN, LOW);
    delay(offMs);
}
#endif

namespace {

// Waits ms with the lowest possible power draw. In debug builds a plain
// delay() is used so the USB serial port stays alive; millis() therefore
// stays meaningful in debug. In release, deepSleep() freezes millis(),
// which is why all long durations below come from the RTC instead.
void lowPowerWait(uint32_t ms) {
#if DEBUG
    delay(ms);
#else
    LowPower.deepSleep(ms);
#endif
}

// Wall-clock source backed by the SAMD21 RTC. The RTC keeps running
// through LowPower.deepSleep() (it is the wake-up source).
class RtcEpochSource : public IEpochClock {
public:
    void begin() { rtc_.begin(); }
    unsigned long epoch() override { return rtc_.getEpoch(); }
    void setEpoch(unsigned long epochSeconds) override {
        rtc_.setEpoch(epochSeconds);
        set_ = true;
    }
    bool isSet() const override { return set_; }

private:
    RTCZero rtc_;
    bool set_ = false;
};

WiFiNetwork wifiNetwork(config::WIFI_SSID, config::WIFI_PASS,
                        config::WIFI_CONNECT_TIMEOUT_MS);
ArduinoHttpClientAdapter httpClient(wifiNetwork, config::HTTP_MAX_BODY_BYTES);

#if INVERTER_BRAND == INVERTER_BRAND_SOLAREDGE
PowerMonitor powerMonitor(std::unique_ptr<IInverterAPI>(new SolarEdgeApiV2(
    config::SOLAREDGE_SITE_ID, config::SOLAREDGE_API_KEY, httpClient,
    config::SOLAREDGE_HOST)));
#else
PowerMonitor powerMonitor(std::unique_ptr<IInverterAPI>(new FroniusSolarApiV1(
    IPAddress(config::INVERTER_IP[0], config::INVERTER_IP[1],
              config::INVERTER_IP[2], config::INVERTER_IP[3]),
    httpClient)));
#endif
SwitchController switchController(config::SWITCH_PIN,
                                    config::POWER_ON_THRESHOLD_W,
                                    config::POWER_OFF_THRESHOLD_W,
                                    config::MIN_ON_TIME_MS,
                                    config::MIN_OFF_TIME_MS,
                                    config::FAILURES_BEFORE_FORCED_OFF);
RtcEpochSource rtcEpochSource;
TimeManager timeManager(rtcEpochSource, config::UTC_OFFSET_SECONDS);

#if BACKUP_ENABLED
OpenMeteoForecast sunshineForecast(config::WEATHER_LATITUDE,
                                    config::WEATHER_LONGITUDE,
                                    config::WEATHER_TIMEZONE, httpClient);
ActivationTracker activationTracker;
BackupChargeConfig backupConfig = {
    config::BACKUP_MIN_ACTIVATION_H,
    config::BACKUP_HISTORY_DAYS,
    config::BACKUP_SUNSHINE_THRESHOLD_H,
    config::BACKUP_START_HOUR,
    config::BACKUP_CHARGE_DURATION_H,
    config::WEATHER_FETCH_HOUR,
};
BackupChargeController backupCharge(sunshineForecast, backupConfig);
bool backupWindowWasActive = false;
#endif

// Wall-clock milliseconds for SwitchController timing.
// RTC-based once synchronized (deep-sleep-proof); millis() before that.
// The one-shot domain switch at the first NTP sync can only let a single
// early or late switch decision slip through: the hysteresis bounds the
// impact to one poll period.
uint32_t wallClockMs() {
    if (rtcEpochSource.isSet()) {
        return (uint32_t)(rtcEpochSource.epoch() * 1000UL);
    }
    return millis();
}

bool attemptNtpSync() {
    timeManager.markSyncAttempt();
    WiFiUDP udp;
    unsigned long epoch = 0;
    if (!NtpSync::sync(udp, config::NTP_SERVER, epoch, config::NTP_TIMEOUT_MS)) {
        LOG_ERROR("NTP", "sync failed");
        return false;
    }
    timeManager.onNtpSync(epoch);
    LOG_VALUE("NTP", "synced, epoch=", epoch);
    return true;
}

bool ensureNetwork() {
    if (wifiNetwork.isConnected()) return true;
    LOG_INFO("WiFi", "connecting...");
    const bool ok = wifiNetwork.connect();
    if (!ok) {
        LOG_ERROR("WiFi", "connection failed");
    } else {
        LOG_VALUE("WiFi", "connected, signal ", wifiNetwork.getSignalQuality());
    }
    return ok;
}

} // namespace

void setup() {
#if DEBUG
    Serial.begin(115200);
    // Give the operator a chance to attach the console; some hosts also
    // need a read before the port is enumerated.
    const unsigned long serialDeadline = millis() + 4000;
    while (!Serial && (long)(serialDeadline - millis()) > 0) {
        delay(100);
    }
    pinMode(LED_BUILTIN, OUTPUT);
#endif

    rtcEpochSource.begin(); // must run before the first deepSleep alarm
    switchController.begin(wallClockMs());

    if (!ensureNetwork()) {
        // No network at boot: keep the switch OFF (fail-safe) and retry
        // from the main loop at a low-power cadence.
        switchController.applyReading(
            Result<int, PowerError>::err(PowerError::NetworkDown), wallClockMs());
    }
    attemptNtpSync();
}

void loop() {
    // 1. Measure production (bounded retries inside PowerMonitor).
    const auto reading = powerMonitor.getOutputPower();
    if (reading.isOk()) {
        LOG_VALUE("PWR", "W = ", reading.unwrap());
    }

    // 2. Drive the switch (hysteresis, min durations, fail-safe).
    //    A scheduled backup charge overrides the production logic:
    //    the boiler then heats from the grid, deliberately.
    const uint32_t nowMs = wallClockMs();
#if BACKUP_ENABLED
    const unsigned long localEpoch = timeManager.localEpoch();
    backupCharge.tick(localEpoch, activationTracker);
    if (backupCharge.shouldForceOn(localEpoch)) {
        switchController.forceOn(nowMs);
        backupWindowWasActive = true;
        LOG_INFO("BACKUP", "forced grid charge");
    } else {
        if (backupWindowWasActive) {
            switchController.forceOff(nowMs); // window just ended
            backupWindowWasActive = false;
        }
        switchController.applyReading(reading, nowMs);
    }
    if (localEpoch != 0) {
        activationTracker.record(localEpoch, switchController.isOn());
    }
#else
    switchController.applyReading(reading, nowMs);
#endif

    // 3. Keep time synchronized at a low rate (RTC-based, once synced).
    if (timeManager.shouldAttemptSync(config::NTP_SYNC_INTERVAL_S,
                                      config::NTP_RETRY_INTERVAL_MS)) {
        attemptNtpSync();
    }

    // 4. Night: cut the boiler, power the radio down, deep sleep.
    if (timeManager.isNight(config::SLEEP_START_HOUR)) {
        LOG_INFO("SLEEP", "night sleep");
        switchController.forceOff(wallClockMs());
        wifiNetwork.disconnect(); // NINA module fully powered down
        lowPowerWait(config::NIGHT_SLEEP_DURATION_MS);
        // The RTC kept running: time is still valid after the wake.
        if (!ensureNetwork()) {
            lowPowerWait(config::NETWORK_FAILURE_RETRY_MS);
        } else {
            attemptNtpSync();
        }
        return;
    }

    // 5. Network lost during the day: low-power backoff, switch fail-safe
    //    will engage after enough consecutive failed polls.
    if (!ensureNetwork()) {
        switchController.applyReading(
            Result<int, PowerError>::err(PowerError::NetworkDown), wallClockMs());
        lowPowerWait(config::NETWORK_FAILURE_RETRY_MS);
        return;
    }

    // 6. Day: sleep until the next poll. The radio stays associated in
    //    DTIM power save (enabled in WiFiNetwork::connect).
#if DEBUG
    blinkLed(1000, 1000);
#else
    digitalWrite(LED_BUILTIN, LOW);
#endif
    lowPowerWait(config::DELAY_BETWEEN_CHECKS_MS);
}
