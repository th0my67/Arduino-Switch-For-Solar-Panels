#ifndef I_HTTP_CLIENT_H
#define I_HTTP_CLIENT_H

#include "utils/result.h"
#include <Arduino.h>

enum class HttpError {
    NetworkFailure,
    Timeout,
    InvalidResponse,
    ServerError,
    NotFound,
    ClientError,
    AuthError,       // 401 / 403: bad credentials
    RateLimited,     // 429
    TlsUnsupported   // https:// requested but no secure client available
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // Performs a GET on an http:// or https:// URL and returns the
    // response body. Implementations must bound both the timeout and
    // the response size.
    virtual Result<String, HttpError> get(
        const char* url,
        unsigned long timeoutMs = 5000
    ) = 0;
};

#endif
