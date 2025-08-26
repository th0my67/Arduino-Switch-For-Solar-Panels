#include "hal/interfaces/i_inverter_api.h"
#include "api/fronius_solar_api_v1.h"
#include "utils/result.h"
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <memory>


FroniusSolarApiV1::FroniusSolarApiV1(const IPAddress ipAddress) : ipAddress_(ipAddress), httpClient_(std::move(HttpClient(,ipAddress))) {}

Result<float, InverterError> FroniusSolarApiV1::getCurrentPower() {
    const char endpoint[256] = "/solar_api/v1/GetInverterRealtimeData.cg1";
    const char query[256] = "?Scope=Device&DeviceId=1&DataCollection=CumulationInverterData";

}