# Solar Panels / Inverter Arduino Switch

## What this application does

This firmware runs on an **Arduino MKR WiFi 1010** (also works on a Nano 33
IoT) plugged into your home network. Every 90 seconds it polls the power
currently produced by your solar inverter and drives a 3.3 V signal on
**pin 6** that can trigger an external switch — typically the contactor of a
water boiler:

- the switch goes **ON** as soon as production reaches the ON threshold
  (default 5000 W), so the boiler heats only from solar surplus;
- the switch goes **OFF** below the OFF threshold (default 4500 W). The gap
  between the two thresholds is an hysteresis: the relay never chatters
  when production oscillates around the limit;
- minimum ON and OFF durations (5 min by default) protect the relay and
  the boiler from rapid cycling;
- if the inverter cannot be read for several polls in a row, the switch is
  forced **OFF** (fail-safe: the boiler must never heat from grid power
  unknowingly);
- at night (from 21:00 by default) the switch is cut, the WiFi module is
  powered down and the board deep-sleeps until morning.

### Supported inverters

Two inverter backends are compiled in, selected by a single constant in
`src/config.h` (`INVERTER_BRAND`):

| Brand | API | Where | Transport |
|---|---|---|---|
| **Fronius** (default) | Solar API v1 `GetInverterRealtimeData` | inverter IP on the LAN | HTTP, no authentication |
| **SolarEdge** | Monitoring API v2 `power-flow` | `monitoringapi.solaredge.com` (cloud) | HTTPS + site API key |

Practical differences:

- **Fronius**: purely local. No Internet access needed, values are
  instantaneous. Point `INVERTER_IP` at the inverter.
- **SolarEdge**: cloud. The board needs Internet access and a site API key
  (monitoring portal, "site details", API access). Values are delayed by
  the SolarEdge platform (typically a few minutes), and each poll performs
  a TLS handshake, which costs a little more energy than local HTTP. The
  payload unit (`W`/`kW`/`MW`) is honored automatically — an unknown or
  missing unit is treated as an error rather than guessed.

## Architecture

```
src/
├── main.cpp                     # orchestration + power management
├── config.h                     # all tunables in one place
├── api/
│   ├── fronius_solar_api_v1.*   # Fronius local Solar API v1 client
│   └── solaredge_api_v2.*       # SolarEdge cloud Monitoring API v2 client
├── services/power_monitor.*     # bounded retries, error mapping, caching
├── switch_controller.*          # hysteresis, min durations, fail-safe OFF
├── time_manager.*               # SNTP client + night detection (RTC-backed)
├── hal/
│   ├── interfaces/              # IInverterAPI, IHttpClient, INetworkInterface
│   └── implementations/         # WiFiNINA / HTTP adapter (+ Ethernet build)
└── utils/                       # Result<T,E>, URL parser, logging
```

Both API clients implement the same `IInverterAPI` interface
(`getCurrentPower()` / `getStatus()`), return a `Result<int, InverterError>`
and never abort: every failure (network, timeout, malformed JSON, rejected
API key, rate limit) is a typed error the caller can act on.

## Low power

The board is asleep almost all the time:

- between polls (90 s): the CPU deep-sleeps; the radio stays associated in
  DTIM power save (`WiFi.lowPowerMode()`);
- at night (21:00): switch OFF, radio fully powered down (`WiFi.end()`),
  9 h of deep sleep;
- wall-clock time comes from the on-chip RTC, which keeps running while
  the CPU sleeps (it is also the wake-up source). `millis()` freezes in
  deep sleep, so it is never used for durations;
- if the network is unreachable, retries back off (10 min) instead of
  busy-looping;
- release builds contain no serial logging and no LED activity.

Typical consumption on a MKR WiFi 1010: a few mA while polling, dominated
by the radio; micro-amps range for the CPU during standby sleep.

## Parameters (src/config.h)

### Inverter selection

| Parameter | Default | Description |
|---|---|---|
| `INVERTER_BRAND` | `INVERTER_BRAND_FRONIUS` | `INVERTER_BRAND_FRONIUS` or `INVERTER_BRAND_SOLAREDGE`. The `mkr1010_solaredge` build environment selects SolarEdge via a compiler flag. |

### Fronius (local API)

| Parameter | Default | Description |
|---|---|---|
| `INVERTER_IP` | `192.168.1.161` | LAN IP address of the inverter. |

### SolarEdge (cloud API)

| Parameter | Default | Description |
|---|---|---|
| `SOLAREDGE_SITE_ID` | `1234567` | Numeric site id from the monitoring portal (in `arduino_secrets.h`: `SECRET_SOLAREDGE_SITE_ID`). |
| `SOLAREDGE_API_KEY` | placeholder | Site API key (in `arduino_secrets.h`: `SECRET_SOLAREDGE_API_KEY`). |
| `SOLAREDGE_HOST` | `monitoringapi.solaredge.com` | API host; overridable for testing. |

### Switch / boiler

| Parameter | Default | Description |
|---|---|---|
| `SWITCH_PIN` | `6` | Digital pin emitting the 3.3 V signal. |
| `POWER_ON_THRESHOLD_W` | `5000` | Production (W) at/above which the switch turns ON. |
| `POWER_OFF_THRESHOLD_W` | `4500` | Production (W) at/below which it turns OFF. Must be below the ON threshold (an inverted pair is auto-normalized at startup). |
| `MIN_ON_TIME_MS` | `300000` | Minimum time the switch stays ON (5 min). |
| `MIN_OFF_TIME_MS` | `300000` | Minimum time it stays OFF (5 min). |
| `FAILURES_BEFORE_FORCED_OFF` | `5` | Consecutive failed readings that force the switch OFF (fail-safe). |

### Polling and night sleep

| Parameter | Default | Description |
|---|---|---|
| `DELAY_BETWEEN_CHECKS_MS` | `90000` | Interval between two production polls (90 s). |
| `SLEEP_START_HOUR` | `21` | Local hour from which the night sleep starts (inclusive; the guard rejects 0 and >= 24). |
| `NIGHT_SLEEP_DURATION_MS` | `32400000` | Night deep-sleep length (9 h). |
| `NETWORK_FAILURE_RETRY_MS` | `600000` | Low-power backoff before retrying a lost network (10 min). |

### Time synchronization

| Parameter | Default | Description |
|---|---|---|
| `NTP_SERVER` | `pool.ntp.org` | SNTP server (UDP 123). |
| `UTC_OFFSET_SECONDS` | `3600` | Local hour offset (CET). DST is not tracked: the night bound shifts by 1 h for a few weeks a year. |
| `NTP_SYNC_INTERVAL_S` | `86400` | Resync period once synchronized (RTC-based). |
| `NTP_RETRY_INTERVAL_MS` | `1800000` | Retry period while unsynchronized (30 min). |
| `NTP_TIMEOUT_MS` | `5000` | SNTP response timeout. |

### Network and HTTP

| Parameter | Default | Description |
|---|---|---|
| `WIFI_SSID` / `WIFI_PASS` | from secrets | WiFi credentials (`SECRET_SSID`, `SECRET_PASS` in `arduino_secrets.h`). |
| `WIFI_CONNECT_TIMEOUT_MS` | `20000` | Maximum blocking time to join WiFi. A rejected password fails immediately instead of retrying. |
| `HTTP_TIMEOUT_MS` | `5000` | Per-request timeout (connection, response, body). |
| `HTTP_MAX_BODY_BYTES` | `4096` | Response size cap — protects the 32 KB RAM of the SAMD21. |

## Build and test

Requirements: [PlatformIO](https://platformio.org/).

```bash
tools/build.sh            # unit tests + all firmware builds
pio test -e native_test   # 117 host unit tests
tools/upload.sh                          # upload Fronius release firmware
tools/upload.sh mkr1010_solaredge       # upload SolarEdge firmware
pio device monitor                      # serial console (debug env)
```

Unit tests run on the host against small Arduino stubs (`test/arduino_stubs`)
with the real ArduinoJson and ArduinoHttpClient libraries: parsing (all
malformed/absent-field variants), both unit conversions (kW/W/MW), URL
validation, HTTP classification (401/403/404/429/5xx, TLS, truncated or
oversized bodies), retry bounds, hysteresis, rollover, NTP packet
validation and night detection.

### Build environments (platformio.ini)

| Environment | Purpose |
|---|---|
| `mkr1010_release` | Fronius production build, `-Os`, no logging |
| `mkr1010_solaredge` | SolarEdge production build (HTTPS) |
| `mkr1010_debug` | verbose serial + LED patterns |
| `mkr1010_ethernet` | adds the Ethernet (SPI shield) backend |
| `mkr1010_ota` | links the OTA library for remote updates |
| `native_test` | host unit tests |

Debug LED patterns (debug env only):

- ON 4s / OFF 4s : no connection to the host
- ON 1s / OFF 4s : waiting for the next request
- ON 0.1s / OFF 0.1s : waiting for the response

At night the LED stays off while the board sleeps.

## Setup

1. Copy `src/arduino_secrets_example.h` to `src/arduino_secrets.h`
   (gitignored) and fill in your WiFi credentials and — for SolarEdge —
   your site id and API key.
2. Pick `INVERTER_BRAND` in `src/config.h` and check the inverter
   parameters.
3. `tools/upload.sh` (or the `mkr1010_solaredge` variant).

The legacy single-file implementation is kept in `main/` for reference;
it is not part of the build.

## Warning

This drives mains-powered equipment through an external switch. Make sure
your wiring, relay and isolation are appropriate before connecting any
load.

## License

This project is provided under the terms of the MIT License.
