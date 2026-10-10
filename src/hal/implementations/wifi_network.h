#ifndef WIFI_NETWORK_H
#define WIFI_NETWORK_H

#include "hal/interfaces/i_network_interface.h"
#include <WiFiNINA.h>

// WiFiNINA implementation. The board (MKR WiFi 1010 / Nano 33 IoT) hosts
// the radio in a u-blox NINA-W102 module; disconnect() powers it down and
// enableLowPowerMode() puts it in DTIM power save (association kept).
class WiFiNetwork : public INetworkInterface {
public:
    WiFiNetwork(const char* ssid, const char* password,
                uint32_t connectTimeoutMs = 20000);

    bool connect() override;      // blocking, bounded by connectTimeoutMs
    bool isConnected() const override;
    void disconnect() override;   // disconnect + end(): module powered down
    Client* createClient() override { return &client_; }
    Client* createSecureClient() override { return &sslClient_; }
    const char* getConnectionType() const override { return "WiFi"; }
    int getSignalQuality() const override;

    // Modem power save between polls; call once, right after connect().
    void enableLowPowerMode() const;

private:
    const char* ssid_;
    const char* password_;
    uint32_t connectTimeoutMs_;
    WiFiClient client_;
    WiFiSSLClient sslClient_; // TLS client for https:// (cloud APIs)
};

#endif
