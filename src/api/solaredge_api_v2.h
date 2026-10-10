#ifndef SOLAREDGE_API_V2_H
#define SOLAREDGE_API_V2_H

#include "hal/interfaces/i_inverter_api.h"
#include "hal/interfaces/i_http_client.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Client for the SolarEdge Monitoring API v2 (cloud, HTTPS).
//
// Reads the site power flow:
//   GET https://<host>/v2/sites/<siteId>/power-flow?api_key=<apiKey>
//
// The payload carries its own unit field ("W", "kW", ...), which is
// honored here and converted to watts. The known SolarEdge quirk is
// that different endpoints answer in different units; a payload without
// a unit is rejected as invalid rather than guessed.
//
// Note: this is the cloud API, so the board needs Internet access and
// values are delayed by the monitoring platform (no local polling).
class SolarEdgeApiV2 : public IInverterAPI {
public:
    SolarEdgeApiV2(const char* siteId, const char* apiKey,
                   IHttpClient& httpClient,
                   const char* host = "monitoringapi.solaredge.com");
    virtual ~SolarEdgeApiV2() = default;

    Result<int, InverterError> getCurrentPower() override;
    Result<InverterStatus, InverterError> getStatus() override;
    const char* getBrandName() const override { return "SolarEdge"; }

    // Extracts the PV production in watts from a power-flow payload.
    // Returns false when the unit is missing/unknown, PV is absent,
    // or the value would overflow an int.
    static bool extractPvPowerW(const JsonDocument& doc, int& powerOut);

    // Maps the PV "status" string ("Active"/"Idle"/...) to an
    // InverterStatus; anything unknown maps to Offline.
    static InverterStatus statusFromPvStatus(const char* pvStatus);

    static InverterError mapHttpError(HttpError error);

private:
    String url_;               // prebuilt once in the constructor
    IHttpClient* httpClient_;
    bool valid_;

    Result<String, HttpError> fetchBody();
};

#endif
