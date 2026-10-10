#include "api/solaredge_api_v2.h"
#include "utils/result.h"
#include <cstring>

SolarEdgeApiV2::SolarEdgeApiV2(const char* siteId, const char* apiKey,
                               IHttpClient& httpClient, const char* host)
    : httpClient_(&httpClient) {
    valid_ = siteId != nullptr && siteId[0] != '\0' &&
             apiKey != nullptr && apiKey[0] != '\0' &&
             host != nullptr && host[0] != '\0';
    if (!valid_) {
        return; // calls will fail fast with InvalidResponse
    }
    // Built once in the constructor: every poll reuses the same string.
    url_ = String("https://") + String(host) +
           String("/v2/sites/") + String(siteId) +
           String("/power-flow?api_key=") + String(apiKey);
}

Result<String, HttpError> SolarEdgeApiV2::fetchBody() {
    if (!valid_) {
        return Result<String, HttpError>::err(HttpError::InvalidResponse);
    }
    return httpClient_->get(url_.c_str());
}

bool SolarEdgeApiV2::extractPvPowerW(const JsonDocument& doc, int& powerOut) {
    JsonVariantConst flow = doc["siteCurrentPowerFlow"];
    if (flow.isNull()) return false;

    JsonVariantConst pv = flow["PV"]["currentPower"];
    if (pv.isNull()) return false;

    double value = 0.0;
    if (pv.is<int>()) {
        value = (double)pv.as<int>();
    } else if (pv.is<double>()) {
        value = pv.as<double>();
    } else {
        return false; // string / bool / nested object: not a power reading
    }

    // The unit is part of the payload and must be honored; different
    // SolarEdge endpoints answer in different units.
    JsonVariantConst unit = flow["unit"];
    if (unit.isNull() || !unit.is<const char*>()) return false;
    const char* u = unit.as<const char*>();
    double multiplier = 0.0;
    if (strcmp(u, "W") == 0) {
        multiplier = 1.0;
    } else if (strcmp(u, "kW") == 0) {
        multiplier = 1000.0;
    } else if (strcmp(u, "MW") == 0) {
        multiplier = 1000000.0;
    } else {
        return false; // unknown unit: never guess
    }

    const double watts = value * multiplier;
    if (watts > 2147483000.0 || watts < -2147483000.0) return false;
    powerOut = (int)(watts >= 0.0 ? watts + 0.5 : watts - 0.5);
    return true;
}

InverterStatus SolarEdgeApiV2::statusFromPvStatus(const char* pvStatus) {
    if (pvStatus == nullptr) return InverterStatus::Offline;
    if (strcmp(pvStatus, "Active") == 0) return InverterStatus::Producing;
    if (strcmp(pvStatus, "Idle") == 0) return InverterStatus::Idle;
    return InverterStatus::Offline; // unknown: do not guess
}

InverterError SolarEdgeApiV2::mapHttpError(HttpError error) {
    switch (error) {
        case HttpError::Timeout:
        case HttpError::NetworkFailure:
            return InverterError::NetworkFailure;
        case HttpError::ServerError:
            return InverterError::ServiceUnavailable;
        case HttpError::AuthError:
            // 401/403: rejected API key.
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

Result<int, InverterError> SolarEdgeApiV2::getCurrentPower() {
    auto body = fetchBody();
    if (body.isError()) {
        return Result<int, InverterError>::err(mapHttpError(body.unwrapErr()));
    }

    // Filtered parse: only what we need survives, keeping the document
    // small even when the platform returns extra fields.
    JsonDocument filter;
    filter["siteCurrentPowerFlow"]["PV"]["currentPower"] = true;
    filter["siteCurrentPowerFlow"]["PV"]["status"] = true;
    filter["siteCurrentPowerFlow"]["unit"] = true;

    JsonDocument doc;
    const DeserializationError jsonError =
        deserializeJson(doc, body.unwrap().c_str(),
                        DeserializationOption::Filter(filter));
    if (jsonError != DeserializationError::Ok) {
        return Result<int, InverterError>::err(InverterError::InvalidResponse);
    }

    int powerW = 0;
    if (!extractPvPowerW(doc, powerW)) {
        return Result<int, InverterError>::err(InverterError::InvalidResponse);
    }

    return Result<int, InverterError>::ok(powerW);
}

Result<InverterStatus, InverterError> SolarEdgeApiV2::getStatus() {
    auto body = fetchBody();
    if (body.isError()) {
        return Result<InverterStatus, InverterError>::err(
            mapHttpError(body.unwrapErr()));
    }

    JsonDocument filter;
    filter["siteCurrentPowerFlow"]["PV"]["currentPower"] = true;
    filter["siteCurrentPowerFlow"]["PV"]["status"] = true;
    filter["siteCurrentPowerFlow"]["unit"] = true;

    JsonDocument doc;
    const DeserializationError jsonError =
        deserializeJson(doc, body.unwrap().c_str(),
                        DeserializationOption::Filter(filter));
    if (jsonError != DeserializationError::Ok) {
        return Result<InverterStatus, InverterError>::err(
            InverterError::InvalidResponse);
    }

    JsonVariantConst pvStatus = doc["siteCurrentPowerFlow"]["PV"]["status"];
    if (!pvStatus.isNull() && pvStatus.is<const char*>()) {
        return Result<InverterStatus, InverterError>::ok(
            statusFromPvStatus(pvStatus.as<const char*>()));
    }

    // No PV status: fall back to the power reading ( Producing / Idle / Error).
    int powerW = 0;
    if (!extractPvPowerW(doc, powerW)) {
        return Result<InverterStatus, InverterError>::err(
            InverterError::InvalidResponse);
    }
    if (powerW > 0) return Result<InverterStatus, InverterError>::ok(InverterStatus::Producing);
    if (powerW == 0) return Result<InverterStatus, InverterError>::ok(InverterStatus::Idle);
    return Result<InverterStatus, InverterError>::ok(InverterStatus::Error);
}
