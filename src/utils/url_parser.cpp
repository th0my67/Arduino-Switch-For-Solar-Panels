#include "utils/url_parser.h"

namespace {
constexpr size_t kMaxUrlLength = 256;

bool isValidHostname(const String& host) {
    if (host.isEmpty()) return false;
    for (unsigned int i = 0; i < host.length(); ++i) {
        const char c = host.charAt(i);
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

// Strict port parse: every character must be a digit, value in [1, 65535].
bool isValidPort(const String& portStr, int& portOut) {
    if (portStr.isEmpty() || portStr.length() > 5) return false;
    long value = 0;
    for (unsigned int i = 0; i < portStr.length(); ++i) {
        const char c = portStr.charAt(i);
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
        if (value > 65535) return false;
    }
    if (value < 1) return false;
    portOut = (int)value;
    return true;
}
} // namespace

UrlComponents parseUrl(const char* url) {
    UrlComponents result;
    result.isValid = false;

    if (url == nullptr) return result;

    const size_t len = strlen(url);
    if (len == 0 || len > kMaxUrlLength) return result;

    String working(url);
    if (working.startsWith("http://")) {
        working = working.substring(7);
    } else {
        // https:// and every other scheme (or scheme-less input) is invalid.
        return result;
    }
    if (working.isEmpty()) return result;

    const int pathIndex = working.indexOf('/');
    String hostPort = (pathIndex == -1) ? working : working.substring(0, pathIndex);
    result.path = (pathIndex == -1) ? String("/") : working.substring(pathIndex);

    const int portIndex = hostPort.indexOf(':');
    if (portIndex != -1) {
        const String hostStr = hostPort.substring(0, portIndex);
        const String portStr = hostPort.substring(portIndex + 1);
        if (!isValidPort(portStr, result.port)) return result;
        hostPort = hostStr;
    }

    if (hostPort.isEmpty()) return result;

    if (result.ip.fromString(hostPort)) {
        result.host = hostPort;
        result.isIp = true;
        result.isValid = true;
        return result;
    }

    if (isValidHostname(hostPort)) {
        result.host = hostPort;
        result.isIp = false;
        result.isValid = true;
        return result;
    }

    return result;
}
