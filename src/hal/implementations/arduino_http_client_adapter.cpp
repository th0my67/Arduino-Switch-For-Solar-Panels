#include "hal/implementations/arduino_http_client_adapter.h"
#include "utils/url_parser.h"
#include <ArduinoHttpClient.h>

namespace {
// ArduinoHttpClient error codes (HTTP_SUCCESS == 0).
HttpError mapRequestError(int err) {
    switch (err) {
        case HTTP_ERROR_TIMED_OUT:
            return HttpError::Timeout;
        case HTTP_ERROR_INVALID_RESPONSE:
            return HttpError::InvalidResponse;
        default: // HTTP_ERROR_CONNECTION_FAILED, HTTP_ERROR_API, unknown
            return HttpError::NetworkFailure;
    }
}
} // namespace

ArduinoHttpClientAdapter::ArduinoHttpClientAdapter(INetworkInterface& network,
                                                   size_t maxBodyBytes)
    : network_(&network), maxBodyBytes_(maxBodyBytes) {}

bool ArduinoHttpClientAdapter::isStatusSuccess(int statusCode) {
    return statusCode >= 200 && statusCode < 400;
}

HttpError ArduinoHttpClientAdapter::classifyStatus(int statusCode) {
    if (statusCode >= 500) return HttpError::ServerError;
    if (statusCode == 404) return HttpError::NotFound;
    if (statusCode == 401 || statusCode == 403) return HttpError::AuthError;
    if (statusCode == 429) return HttpError::RateLimited;
    return HttpError::ClientError; // other 4xx
}

Result<String, HttpError> ArduinoHttpClientAdapter::get(const char* url,
                                                        unsigned long timeoutMs) {
    const UrlComponents parsed = parseUrl(url);
    if (!parsed.isValid) {
        return Result<String, HttpError>::err(HttpError::InvalidResponse);
    }

    if (!network_->isConnected() && !network_->connect()) {
        return Result<String, HttpError>::err(HttpError::NetworkFailure);
    }

    Client* rawClient = parsed.secure ? network_->createSecureClient()
                                     : network_->createClient();
    if (rawClient == nullptr) {
        // https:// requested, but this backend has no TLS client.
        return Result<String, HttpError>::err(HttpError::TlsUnsupported);
    }

    HttpClient http(*rawClient, parsed.host, (uint16_t)parsed.port);
    http.setHttpResponseTimeout(timeoutMs);

    // Guarantees the connection is closed on every exit path.
    struct StopGuard {
        HttpClient& h;
        ~StopGuard() { h.stop(); }
    } stopGuard{http};

    const int requestError = http.get(parsed.path.c_str());
    if (requestError != HTTP_SUCCESS) {
        return Result<String, HttpError>::err(mapRequestError(requestError));
    }

    const int statusCode = http.responseStatusCode();
    if (statusCode < 0) {
        return Result<String, HttpError>::err(mapRequestError(statusCode));
    }
    if (!isStatusSuccess(statusCode)) {
        return Result<String, HttpError>::err(classifyStatus(statusCode));
    }

    const int contentLength = http.contentLength(); // -1 when absent/chunked
    if (contentLength > 0 && (size_t)contentLength > maxBodyBytes_) {
        return Result<String, HttpError>::err(HttpError::InvalidResponse);
    }

    // Bounded body read. Without Content-Length the read stops when the
    // server closes the connection or the overall deadline expires.
    const unsigned long deadline = millis() + timeoutMs;
    String body;
    while (true) {
        while (http.available() != 0) {
            const int c = http.read();
            if (c < 0) break;
            body += (char)c;
            if (body.length() >= maxBodyBytes_) {
                return Result<String, HttpError>::err(HttpError::InvalidResponse);
            }
        }
        if (contentLength > 0 && (int)body.length() >= contentLength) break;
        if (!http.connected()) break; // server closed the connection
        if ((long)(deadline - millis()) <= 0) {
            return Result<String, HttpError>::err(HttpError::Timeout);
        }
        delay(1); // yield while waiting for data in flight
    }

    if (contentLength > 0 && (int)body.length() < contentLength) {
        return Result<String, HttpError>::err(HttpError::InvalidResponse);
    }

    return Result<String, HttpError>::ok(body);
}
