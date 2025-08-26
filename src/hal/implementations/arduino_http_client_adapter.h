#ifndef ARDUINO_HTTP_CLIENT_ADAPTER_H
#define ARDUINO_HTTP_CLIENT_ADAPTER_H

#include "hal/interfaces/i_http_client.h"
#include "hal/interfaces/i_network_interface.h"
#include <ArduinoHttpClient.h>

class ArduinoHttpClientAdapter : public IHttpClient {
public:
    ArduinoHttpClientAdapter(INetworkInterface* network);
    ~ArduinoHttpClientAdapter() = default;
    
    Result<String, HttpError> get(const char* url , int timeoutMs = 5000) override;
private:
    INetworkInterface* network_;
    
    struct ParsedUrl {
        IPAddress host;
        int port;
        String path;
        bool isValid;
    };
    
    ParsedUrl parseUrl(const char* url);
    HttpError mapArduinoError(int errorCode);
};

#endif