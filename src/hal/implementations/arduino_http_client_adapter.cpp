#include "utils/result.h"
#include "hal/implementations/arduino_http_client_adapter.h"
#include <ArduinoHttpClient.h>


ArduinoHttpClientAdapter::ArduinoHttpClientAdapter(INetworkInterface* network) : network_(network) {}

Result<String, HttpError> ArduinoHttpClientAdapter::get(const char* url, int timeoutMs) {
    if (!network_->isConnected()) {
        if (!network_->connect()) {
            return HttpError::NetworkFailure;
        }
    }
    
    auto parsedUrl = parseUrl(url);
    if (!parsedUrl.isValid) {
        return HttpError::InvalidResponse;
    }
    
    auto client = network_->createClient();
    if (!client) {
        return HttpError::NetworkFailure;
    }
    
    HttpClient httpClient(client, parsedUrl.host, parsedUrl.port);
    httpClient.setTimeout(timeoutMs);
    
    int err = httpClient.get(parsedUrl.path);
    if (err != 0) {
        return mapArduinoError(err);
    }
    
    int statusCode = httpClient.responseStatusCode();
    if (statusCode >= 400 && statusCode < 500) {
        return HttpError::NotFound;
    } else if (statusCode >= 500) {
        return HttpError::ServerError;
    }
    
    String responseBody = httpClient.responseBody();
    
    return responseBody;
}


ArduinoHttpClientAdapter::ParsedUrl ArduinoHttpClientAdapter::parseUrl(const char* url) {
    ParsedUrl result;
    result.isValid = false;
    result.port = 80;  // Default port
    result.path = "/"; // Default path
   
    // Input validation
    if (!url || strlen(url) == 0) {
        return result;
    }
    
    // Convert to String for easier manipulation
    String workingUrl = String(url);
    
    // Basic URL parsing (http://<host>:<port>/<path>)
    if (workingUrl.startsWith("http://")) {
        workingUrl = workingUrl.substring(7);  // Remove "http://"
    } else if (workingUrl.startsWith("https://")) {
        // HTTPS not supported in this basic adapter
        return result;
    } else {
        // No protocol specified - invalid
        return result;
    }
   
    // Split into host:port and path parts
    int pathIndex = workingUrl.indexOf('/');
    String hostPort = (pathIndex == -1) ? workingUrl : workingUrl.substring(0, pathIndex);
    result.path = (pathIndex == -1) ? "/" : workingUrl.substring(pathIndex);
   
    // Parse host and port
    int portIndex = hostPort.indexOf(':');
    String hostStr = (portIndex == -1) ? hostPort : hostPort.substring(0, portIndex);
    String portStr = (portIndex == -1) ? "80" : hostPort.substring(portIndex + 1);
    
    // Parse port number with validation
    if (portStr.length() > 0) {
        long portLong = portStr.toInt();
        if (portLong > 0 && portLong <= 65535) {
            result.port = (int)portLong;
        } else {
            // Invalid port number
            return result;
        }
    }
    
    // Parse IP address (most critical part)
    if (hostStr.length() > 0) {
        // Try to parse as IP address
        if (!result.host.fromString(hostStr)) {
            // Could not parse IP address
            return result;
        }
    } else {
        // Empty host
        return result;
    }
    
    // Validate path
    if (result.path.length() == 0) {
        result.path = "/";
    }
    
    // All validations passed
    result.isValid = true;
    return result;
}
