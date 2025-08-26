#ifndef FRONIUS_SOLAR_API_V1_H
#define FRONIUS_SOLAR_API_V1_H

#include "hal/interfaces/i_inverter_api.h"
#include "utils/result.h"
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <memory>

class FroniusSolarApiV1 : public IInverterAPI {
public:
    FroniusSolarApiV1(const IPAddress ipAddress);
    virtual ~FroniusSolarApiV1() = default;

    Result<float, InverterError> getCurrentPower() override;
    Result<InverterStatus, InverterError> getStatus() override;
    const char* getBrandName() const override { return "Fronius"; }
private:
    const IPAddress ipAddress_;
    const std::unique_ptr<HttpClient> httpClient_;
    InverterStatus parseStatus(const char* statusStr);
    float parsePower(const char* powerStr);
    Result<JsonDocument, InverterError> fetchData(const char* endpoint);
};


#endif