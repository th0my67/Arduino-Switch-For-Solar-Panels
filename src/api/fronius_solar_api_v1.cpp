#include "api/fronius_solar_api_v1.h"
#include "utils/result.h"

FroniusSolarApiV1::FroniusSolarApiV1(IPAddress ipAddress, IHttpClient& httpClient)
    : ipAddress_(ipAddress), httpClient_(&httpClient) {
    // Built once in the constructor: every poll reuses the same string.
    baseUrl_ = String("http://") + ipAddress_.toString();
}

Result<String, HttpError> FroniusSolarApiV1::fetchBody(const char* endpoint) {
    // baseUrl_ is bounded by the IP address representation; endpoints are
    // compile-time constants, so the total length is known small.
    String url = baseUrl_ + String(endpoint);
    return httpClient_->get(url.c_str());
}

bool FroniusSolarApiV1::extractPower(const JsonDocument& doc, int& powerOut) {
    JsonVariantConst pac = doc["Body"]["Data"]["PAC"]["Value"];
    if (pac.isNull() || pac.is<const char*>()) return false;

    if (pac.is<int>()) {
        powerOut = pac.as<int>();
        return true;
    }
    if (pac.is<double>()) {
        const double value = pac.as<double>();
        if (value > 2147483647.0 || value < -2147483648.0) return false;
        powerOut = (int)value;
        return true;
    }
    return false;
}

InverterStatus FroniusSolarApiV1::statusFromPower(int powerW) {
    if (powerW > 0) return InverterStatus::Producing;
    if (powerW == 0) return InverterStatus::Idle;
    return InverterStatus::Error; // negative PAC is not a valid production
}

InverterError FroniusSolarApiV1::mapHttpError(HttpError error) {
    switch (error) {
        case HttpError::Timeout:
        case HttpError::NetworkFailure:
            return InverterError::NetworkFailure;
        case HttpError::ServerError:
            return InverterError::ServiceUnavailable;
        case HttpError::AuthError:
            return InverterError::AuthenticationFailure;
        case HttpError::RateLimited:
            return InverterError::RateLimited;
        case HttpError::NotFound:
        case HttpError::ClientError:
        case HttpError::InvalidResponse:
        case HttpError::TlsUnsupported:
        default:
            return InverterError::InvalidResponse;
    }
}

Result<int, InverterError> FroniusSolarApiV1::getCurrentPower() {
    static const char kEndpoint[] =
        "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceId=1"
        "&DataCollection=CumulationInverterData";

    auto body = fetchBody(kEndpoint);
    if (body.isError()) {
        return Result<int, InverterError>::err(mapHttpError(body.unwrapErr()));
    }

    // Filtered parse: only Body.Data.PAC.Value is retained, keeping the
    // document small even when the inverter returns extra fields.
    JsonDocument filter;
    filter["Body"]["Data"]["PAC"]["Value"] = true;

    JsonDocument doc;
    const DeserializationError jsonError =
        deserializeJson(doc, body.unwrap().c_str(),
                        DeserializationOption::Filter(filter));
    if (jsonError != DeserializationError::Ok) {
        return Result<int, InverterError>::err(InverterError::InvalidResponse);
    }

    int power = 0;
    if (!extractPower(doc, power)) {
        // Fronius answers {"Body":{"Data":null}} for an unknown device:
        // indistinguishable from a malformed payload for our purpose.
        return Result<int, InverterError>::err(InverterError::InvalidResponse);
    }

    return Result<int, InverterError>::ok(power);
}

Result<InverterStatus, InverterError> FroniusSolarApiV1::getStatus() {
    // Derived from the power reading: locally there is no distinct
    // status endpoint worth an extra request (each request costs energy).
    auto power = getCurrentPower();
    if (power.isError()) {
        return Result<InverterStatus, InverterError>::err(power.unwrapErr());
    }
    return Result<InverterStatus, InverterError>::ok(statusFromPower(power.unwrap()));
}
