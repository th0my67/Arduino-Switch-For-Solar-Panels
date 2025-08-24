#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFiNINA.h>

#if ETHERNET_ENABLED
#include <Ethernet.h>
#endif

enum ConnectionType {
    CONN_NONE,
    CONN_WIFI,
    CONN_ETHERNET
};

class NetworkManager {
public:
    static bool initialize();
    static bool isConnected();
    static void reconnect();
    static int getCurrentTime();
    static ConnectionType getActiveConnection();
    
    // Get the appropriate client for HTTP requests
    static Client* getClient();
    
private:
    static ConnectionType _activeConnection;
    static WiFiClient _wifiClient;
    
#if ETHERNET_ENABLED
    static EthernetClient _ethernetClient;
    static bool initializeEthernet();
#endif
    
    static bool initializeWiFi();
    static void debugLedPattern(bool connected);
};

#endif