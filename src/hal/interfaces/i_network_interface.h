#ifndef I_NETWORK_INTERFACE_H
#define I_NETWORK_INTERFACE_H

#include <WiFiNINA.h>

class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;
    
    virtual bool connect() = 0;
    virtual bool isConnected() const = 0;
    virtual Client* getClient() = 0;
    virtual void disconnect() = 0;
};

#endif