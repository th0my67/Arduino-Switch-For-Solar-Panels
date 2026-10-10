#ifndef ARDUINO_HTTP_CLIENT_ADAPTER_H
#define ARDUINO_HTTP_CLIENT_ADAPTER_H

#include "hal/interfaces/i_http_client.h"
#include "hal/interfaces/i_network_interface.h"

class ArduinoHttpClientAdapter : public IHttpClient {
public:
    explicit ArduinoHttpClientAdapter(INetworkInterface& network,
                                     size_t maxBodyBytes = 4096);

    Result<String, HttpError> get(const char* url,
                                 unsigned long timeoutMs = 5000) override;

    // Maps an HTTP status code to the error domain. 2xx/3xx are success.
    static bool isStatusSuccess(int statusCode);
    static HttpError classifyStatus(int statusCode);
private:
    INetworkInterface* network_;
    size_t maxBodyBytes_;
};

#endif
