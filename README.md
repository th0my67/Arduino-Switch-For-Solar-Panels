# Solar Panels / Inverter Arduino Switch

Activate a switch when the solar inverter produces more power than a
threshold — e.g. start a boiler when production exceeds 5000 W so it heats
only from surplus solar power.

The firmware targets an **Arduino MKR WiFi 1010** (also works on a Nano 33
IoT) and a **Fronius** inverter on the local network (Solar API v1, plain
HTTP, no authentication). It drives a 3.3 V signal on pin 6 when the
inverter is above the ON threshold, and back LOW below the OFF threshold.

## Architecture

The code is split into small, individually tested modules:

```
src/
├── main.cpp                     # orchestration + power management
├── config.h                     # all tunables in one place
├── api/fronius_solar_api_v1.*   # Fronius Solar API v1 client (IInverterAPI)
├── services/power_monitor.*     # bounded retries, error mapping, caching
├── switch_controller.*          # hysteresis, min durations, fail-safe OFF
├── time_manager.*               # SNTP client + night detection (RTC-backed)
├── hal/
│   ├── interfaces/              # IInverterAPI, IHttpClient, INetworkInterface
│   └── implementations/         # WiFiNINA / HTTP adapter (+ Ethernet build)
└── utils/                       # Result<T,E>, URL parser, logging
```

Key behaviors:

- **Hysteresis** around the threshold (ON at 5000 W, OFF at 4500 W) plus
  minimum ON/OFF durations: the relay never chatters when production
  oscillates around the limit.
- **Fail-safe**: after several consecutive failed readings the switch is
  forced OFF, so the boiler never heats from grid power unknowingly.
- **Low energy**:
  - between polls (90 s): the CPU deep-sleeps, the radio stays associated
    in DTIM power save (`WiFi.lowPowerMode()`);
  - at night (21:00, RTC-based): the switch is cut, the radio is powered
    down (`WiFi.end()`) and the board deep-sleeps 9 h;
  - timekeeping uses the on-chip RTC, which keeps running while asleep
    (`millis()` freezes in deep sleep and is never used for wall time);
  - release builds drop serial logging and LED activity entirely.
- **Robustness**: every network/JSON failure path returns a typed error;
  HTTP response sizes and timeouts are bounded (SAMD21: 32 KB RAM).

## Build and test

Requirements: [PlatformIO](https://platformio.org/).

```bash
tools/build.sh            # unit tests + all firmware builds
pio test -e native_test   # native unit tests only (92 cases)
tools/upload.sh           # build + upload the release firmware over USB
pio device monitor        # serial console (debug env)
```

Unit tests run on the host against small Arduino stubs (`test/arduino_stubs`)
with the real ArduinoJson and ArduinoHttpClient libraries, so parsing,
retries, switch logic, NTP parsing and URL validation are all covered,
including edge cases: malformed JSON, oversized or truncated responses,
timeouts, millis() rollover, unsynchronized clock, inverted thresholds.

## Configuration

Everything is in `src/config.h`:

- WiFi credentials: copy the placeholders from
  `src/arduino_secrets_example.h` to `src/arduino_secrets.h` (gitignored)
  and fill them in.
- inverter IP, thresholds, poll interval, night sleep, NTP server and
  UTC offset.

## Environments (platformio.ini)

| Environment          | Purpose                                  |
|----------------------|------------------------------------------|
| `mkr1010_release`    | production build, `-Os`, no logging      |
| `mkr1010_debug`      | verbose serial + LED patterns            |
| `mkr1010_ethernet`   | adds the Ethernet (SPI shield) backend   |
| `mkr1010_ota`        | links the OTA library for remote updates |
| `native_test`        | host unit tests                          |

Debug LED patterns (debug env only):

- ON 4s / OFF 4s : no connection to the host
- ON 1s / OFF 4s : waiting for the next request
- ON 0.1s / OFF 0.1s : waiting for the response

At night the LED stays off while the board sleeps.

The legacy single-file implementation is kept in `main/` for reference;
it is not part of the build.

## Warning

This drives mains-powered equipment through an external switch. Make
sure your wiring, relay and isolation are appropriate before connecting
any load.

## License

This project is provided under the terms of the MIT License.
