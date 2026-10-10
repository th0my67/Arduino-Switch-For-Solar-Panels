#ifndef I_NETWORK_INTERFACE_H
#define I_NETWORK_INTERFACE_H

#include <Arduino.h>

// The SAMD Arduino core keeps Client inside namespace arduino and does not
// export it through <Arduino.h>; forward-declare it so this radio-agnostic
// interface does not need to include WiFiNINA.h.
#if defined(ARDUINO)
namespace arduino {
class Client;
}
using arduino::Client;
#else
class Client; // native tests: defined in test/arduino_stubs/Client.h
#endif

// Abstraction over the physical network (WiFi module, Ethernet shield...).
// Implementations own their Client instance; createClient() returns a
// pointer that remains valid for the lifetime of the implementation object.
class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    virtual bool connect() = 0;
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;
    virtual Client* createClient() = 0;
    // TLS-capable client for https:// URLs. Returns nullptr when the
    // backend has no TLS support (e.g. plain Ethernet).
    virtual Client* createSecureClient() { return nullptr; }

    // Diagnostics
    virtual const char* getConnectionType() const = 0; // "WiFi", "Ethernet"
    virtual int getSignalQuality() const = 0;          // 0-100, -1 if N/A
};

#endif
