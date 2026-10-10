#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <Arduino.h>

// Parsed components of an http:// or https:// URL. Host is either a literal
// IPv4 address (isIp == true) or a hostname (isIp == false). Other schemes
// are rejected (no ftp, ws, or scheme-less input).
struct UrlComponents {
    String host;
    IPAddress ip;
    bool isIp = false;
    bool secure = false; // https
    int port = 80;
    String path = "/";
    bool isValid = false;
};

// Strict URL parser. Rejects: null/empty input, unsupported schemes,
// empty hosts, non-numeric or out-of-range ports, trailing garbage after
// the port, and over-long inputs.
UrlComponents parseUrl(const char* url);

#endif
