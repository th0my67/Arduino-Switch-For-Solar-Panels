#ifndef I_NETWORK_INTERFACE_H
#define I_NETWORK_INTERFACE_H

#include <WiFiNINA.h>
#include "utils/result.h"

enum class NetworkError {
    ConnectionFailed,
    RequestFailed,
    InvalidResponse,
    Timeout
};

class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;
    
    // Pure network operations only
    virtual bool connect() = 0;
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;
    
    // Factory for raw connections
    virtual Client createClient() = 0;
    
    // Network diagnostics
    virtual const char* getConnectionType() const = 0;  // "WiFi", "Ethernet"
    virtual int getSignalQuality() const = 0;           // 0-100, -1 if N/A
};

#endif