# Solar Panels / Inverter Arduino Switch

Activates a switch (e.g. a boiler contactor) when the solar inverter produces
more power than a threshold — and, new in this rewrite, can also activate it at
night when tomorrow's sunshine forecast is poor and the switch barely ran over
the last two days (cheap night tariff fallback).

The code is organised so that the **board** (MKR Zero + ETH Shield, MKR WiFi
1010, …) and the **inverter vendor** (Fronius Solar API v1, SolarEdge
monitoring API v2, …) are interchangeable adapters: the decision logic never
touches hardware or vendor specifics, and runs as unit tests on your PC.
See [ARCHITECTURE.md](ARCHITECTURE.md) (in French) for the full design.

## Features

- **Day mode** — polls the inverter every 90 s; switch ON above 5000 W,
  OFF below 4500 W (hysteresis), fail-safe OFF when the inverter is unreachable.
- **Evening decision (21:00)** — fetches tomorrow's sunshine hours from the
  MeteoSwiss ICON-CH1 model (served as JSON by [Open-Meteo](https://open-meteo.com)).
  If below 4 h **and** the switch ran less than 6 h over the last two days,
  a night activation is scheduled.
- **Night boost (02:00, 3 h)** — switch forced ON, then everything deep-sleeps
  until 06:00. All thresholds and times are configurable in
  `SolarSwitch/src/config/AppConfig.h`.
- **Low power** — SAMD21 standby between polls and through the night; the
  legacy debug LED blink patterns are preserved.

## Supported targets

| | Fronius | SolarEdge |
|---|---|---|
| MKR Zero + MKR ETH Shield | `pio run -e mkrzero_eth` | (needs an NTP time source, see ARCHITECTURE.md) |
| MKR WiFi 1010 | `pio run -e mkrwifi1010` | `pio run -e mkrwifi1010_solaredge` |

Arduino IDE users: open `SolarSwitch/SolarSwitch.ino` and pick the target in
`src/config/PlatformSelect.h`.

## Getting started

1. Edit `SolarSwitch/src/config/AppConfig.h`: inverter IP (Fronius) or site id
   (SolarEdge), your latitude/longitude for the forecast, thresholds and hours.
2. WiFi or SolarEdge builds: copy `SolarSwitch/src/config/Secrets.example.h`
   to `Secrets.h` and fill in your credentials (the file is gitignored).
3. Build and upload with PlatformIO or the Arduino IDE.

## Tests

The whole decision logic (state machine, activation history, boost policy,
JSON parsing, clock) is hardware-independent and covered by host-side tests:

```bash
pio test -e native
```

## Repository layout

- `SolarSwitch/` — the application (see ARCHITECTURE.md for the layer map)
- `test/test_core/` — host-side unit tests
- `legacy/main.ino` — the original single-file sketch, kept for reference

## License

MIT License.
