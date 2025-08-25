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
    virtual bool isConnected() const = 0;
    virtual Client* getClient() = 0;
    virtual Result<String, NetworkError> makeHttpRequest(
        const char* host, 
        int port, 
        const char* path,
        const char* headers = nullptr
    ) = 0;
};

#endif