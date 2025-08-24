#ifndef WIFI_NETWORK_H
#define WIFI_NETWORK_H

#include "../interfaces/i_network_interface.h"
#include <WiFiNINA.h>

// Concrete implementation = "HOW WiFi actually works"
class WiFiNetwork : public INetworkInterface {
public:
    WiFiNetwork(const char* ssid, const char* password);
    
    // Override all pure virtual functions from interface
    bool connect() override;
    bool isConnected() const override;
    Client* getClient() override;
    void disconnect() override;
    
private:
    const char* ssid_;
    const char* password_;
    WiFiClient client_;
};

#endif