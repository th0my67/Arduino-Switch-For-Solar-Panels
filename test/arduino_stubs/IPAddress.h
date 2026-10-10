// Minimal Arduino IPAddress stub for native unit tests.
#ifndef TEST_IPADDRESS_H
#define TEST_IPADDRESS_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "String.h"

class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        v_ = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
        valid_ = true;
    }

    bool fromString(const char* s) {
        if (!s) return false;
        uint32_t parts[4];
        int idx = 0;
        const char* p = s;
        while (idx < 4) {
            if (!*p || !isdigit((unsigned char)*p)) return false;
            long v = 0;
            int digits = 0;
            while (*p && isdigit((unsigned char)*p)) {
                v = v * 10 + (*p - '0');
                ++p; ++digits;
                if (digits > 3 || v > 255) return false;
            }
            parts[idx++] = (uint32_t)v;
            if (idx < 4) {
                if (*p != '.') return false;
                ++p;
            }
        }
        // Must have consumed exactly four dot-separated octets.
        if (idx == 4 && *p != '\0') return false;
        v_ = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
        valid_ = true;
        return true;
    }
    bool fromString(const String& s) { return fromString(s.c_str()); }

    String toString() const {
        if (!valid_) return String();
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 (unsigned)(v_ >> 24), (unsigned)((v_ >> 16) & 0xFF),
                 (unsigned)((v_ >> 8) & 0xFF), (unsigned)(v_ & 0xFF));
        return String(buf);
    }

    uint8_t operator[](int i) const {
        if (i < 0 || i > 3 || !valid_) return 0;
        return (uint8_t)(v_ >> (24 - 8 * i));
    }

    bool operator==(const IPAddress& o) const { return valid_ && o.valid_ && v_ == o.v_; }
    bool operator!=(const IPAddress& o) const { return !(*this == o); }
    operator bool() const { return valid_; }

private:
    uint32_t v_ = 0;
    bool valid_ = false;
};

#endif
