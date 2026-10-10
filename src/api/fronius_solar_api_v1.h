#ifndef FRONIUS_SOLAR_API_V1_H
#define FRONIUS_SOLAR_API_V1_H

#include "hal/interfaces/i_inverter_api.h"
#include "hal/interfaces/i_http_client.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Client for the local Fronius Solar API v1 (GetInverterRealtimeData.cgi).
// The inverter is expected to be reachable over plain HTTP on the local
// network; no authentication is used.
class FroniusSolarApiV1 : public IInverterAPI {
public:
    FroniusSolarApiV1(IPAddress ipAddress, IHttpClient& httpClient);
    virtual ~FroniusSolarApiV1() = default;

    Result<int, InverterError> getCurrentPower() override;
    Result<InverterStatus, InverterError> getStatus() override;
    const char* getBrandName() const override { return "Fronius"; }

    // Extracts the PAC value (watts) from a Solar API realtime response.
    // Returns false when the field is absent, null, or not a number.
    static bool extractPower(const JsonDocument& doc, int& powerOut);

    // Maps an instantaneous power reading to an inverter status.
    static InverterStatus statusFromPower(int powerW);

    static InverterError mapHttpError(HttpError error);

private:
    IPAddress ipAddress_;
    IHttpClient* httpClient_;
    String baseUrl_;
    Result<String, HttpError> fetchBody(const char* endpoint);
};

#endif
