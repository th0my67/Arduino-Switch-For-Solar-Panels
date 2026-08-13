#pragma once

// Build-time selection of the hardware and the inverter vendor. With
// PlatformIO these are set per environment through build_flags (see
// platformio.ini). With the Arduino IDE, edit the defaults below.

// ---- Board ----------------------------------------------------------------
//   SOLAR_PLATFORM_MKR_ETH   — MKR Zero (or any SAMD MKR) + MKR ETH Shield
//   SOLAR_PLATFORM_MKR_WIFI  — MKR WiFi 1010 (requires src/config/Secrets.h)
#if !defined(SOLAR_PLATFORM_MKR_ETH) && !defined(SOLAR_PLATFORM_MKR_WIFI)
#define SOLAR_PLATFORM_MKR_ETH 1
#endif

// ---- Inverter vendor ------------------------------------------------------
//   SOLAR_INVERTER_FRONIUS   — Fronius Solar API v1 (local network, HTTP)
//   SOLAR_INVERTER_SOLAREDGE — SolarEdge monitoring API v2 (cloud, TLS,
//                              requires API keys in src/config/Secrets.h)
#if !defined(SOLAR_INVERTER_FRONIUS) && !defined(SOLAR_INVERTER_SOLAREDGE)
#define SOLAR_INVERTER_FRONIUS 1
#endif
