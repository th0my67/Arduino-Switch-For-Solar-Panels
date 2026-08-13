#include "JsonScanner.h"

#include <stdlib.h>
#include <string.h>

namespace solar {

namespace {
bool isNumberChar(int c) {
  return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
         c == 'e' || c == 'E';
}
bool isJsonWhitespace(int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
}  // namespace

bool JsonScanner::findKey(const char* name) {
  const uint32_t nameLen = (uint32_t)strlen(name);
  const uint32_t patternLen = nameLen + 2;  // '"' + name + '"'
  uint32_t matched = 0;
  while (true) {
    const int c = in_.read();
    if (c < 0) return false;
    // A '"' can only appear at the pattern's edges (JSON keys hold no raw
    // quotes), so the restart logic below stays exact without a full KMP.
    const char expected =
        (matched == 0 || matched == patternLen - 1) ? '"' : name[matched - 1];
    if ((char)c == expected) {
      if (++matched == patternLen) return true;
    } else {
      matched = (c == '"') ? 1 : 0;
    }
  }
}

int JsonScanner::skipUntil(char target) {
  while (true) {
    const int c = in_.read();
    if (c < 0) return -1;
    if ((char)c == target) return c;
  }
}

bool JsonScanner::parseNumber(float& out, int& terminator) {
  int c = in_.read();
  while (c == ':' || isJsonWhitespace(c)) c = in_.read();
  if (c < 0) return false;
  if (c == 'n') {  // "null"
    while (c >= 0 && c != ',' && c != ']' && c != '}') c = in_.read();
    if (c < 0) return false;
    out = 0.0f;
    terminator = c;
    return true;
  }
  if (!isNumberChar(c)) return false;
  char buf[24];
  uint8_t n = 0;
  while (c >= 0 && isNumberChar(c) && n < sizeof(buf) - 1) {
    buf[n++] = (char)c;
    c = in_.read();
  }
  buf[n] = '\0';
  out = (float)atof(buf);
  terminator = c;
  return true;
}

bool JsonScanner::readFloat(float& out) {
  int terminator;
  return parseNumber(out, terminator);
}

bool JsonScanner::readString(char* buf, uint8_t bufLen) {
  if (bufLen == 0 || skipUntil('"') < 0) return false;
  uint8_t n = 0;
  while (true) {
    const int c = in_.read();
    if (c < 0) return false;
    if (c == '"') {
      buf[n] = '\0';
      return true;
    }
    if (n < bufLen - 1) buf[n++] = (char)c;
  }
}

bool JsonScanner::readFloatArray(float* out, uint8_t maxCount, uint8_t& count) {
  count = 0;
  if (skipUntil('[') < 0) return false;
  while (true) {
    float value;
    int terminator;
    if (!parseNumber(value, terminator)) return false;
    if (count < maxCount) out[count++] = value;
    while (terminator != ',' && terminator != ']') {
      if (terminator < 0) return false;
      terminator = in_.read();
    }
    if (terminator == ']') return true;
  }
}

}  // namespace solar
