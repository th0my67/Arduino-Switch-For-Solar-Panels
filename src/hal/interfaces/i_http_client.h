#ifndef I_HTTP_CLIENT_H
#define I_HTTP_CLIENT_H

#include <cstddef>

class IHTTPClient {
public:
    virtual ~IHTTPClient() = default;

    virtual bool get(const char* url, char* responseBuffer, size_t bufferSize) = 0;
};


#endif