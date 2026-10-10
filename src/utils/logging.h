#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>

#if DEBUG
#define LOG_ERROR(tag, msg) do { Serial.print(F("[")); Serial.print(tag); \
        Serial.print(F("] ")); Serial.println(F(msg)); } while (0)
#define LOG_INFO(tag, msg) do { Serial.print(F("[")); Serial.print(tag); \
        Serial.print(F("] ")); Serial.println(F(msg)); } while (0)
#define LOG_VALUE(tag, msg, value) do { Serial.print(F("[")); Serial.print(tag); \
        Serial.print(F("] ")); Serial.print(F(msg)); Serial.println(value); } while (0)
#else
#define LOG_ERROR(tag, msg) do {} while (0)
#define LOG_INFO(tag, msg) do {} while (0)
#define LOG_VALUE(tag, msg, value) do {} while (0)
#endif

#endif
