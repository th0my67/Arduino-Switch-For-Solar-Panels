#include "hal/interfaces/i_inverter_api.h"
#include "hal/interfaces/i_http_client.h"
#include "api/fronius_solar_api_v1.h"
#include "utils/result.h"
#include <ArduinoJson.h>
#include <memory>


FroniusSolarApiV1::FroniusSolarApiV1(const IPAddress ipAddress,  IHttpClient& httpClient) : ipAddress_(ipAddress), httpClient_(std::move(httpClient)) {}

Result<int, InverterError> FroniusSolarApiV1::getCurrentPower() {
    const char endpoint[] = "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceId=1&DataCollection=CumulationInverterData";

    auto result = fetchData(endpoint);
    if (result.isError()) {
        return result.unwrapErr();
    }
    JsonDocument doc = result.unwrap();

    int power = doc["Body"]["Data"]["PAC"]["Value"] | 0;

    return power;
}

Result<JsonDocument, InverterError> FroniusSolarApiV1::fetchData(const char* endpoint){
    String url = String("http://") + ipAddress_.toString() + String(endpoint);
    auto response = httpClient_->get(url.c_str());
    if (response.isError()) {
        return InverterError::NetworkFailure;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response.unwrap());
    if (error) {
        return InverterError::InvalidResponse;
    }

    return doc;
}