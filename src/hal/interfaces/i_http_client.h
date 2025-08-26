#ifndef I_HTTP_CLIENT_H
#define I_HTTP_CLIENT_H

#include "utils/result.h"
#include <Arduino.h>
#include <string_view>

enum class HttpError {
    NetworkFailure,
    Timeout,
    InvalidResponse,
    ServerError,
    NotFound
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    
    virtual Result<String, HttpError> get(
        const char* url,
        int timeoutMs = 5000
    ) = 0;

};


#endif