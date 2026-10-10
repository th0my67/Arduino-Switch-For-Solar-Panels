#include "hal/implementations/wifi_network.h"

WiFiNetwork::WiFiNetwork(const char* ssid, const char* password,
                         uint32_t connectTimeoutMs)
    : ssid_(ssid), password_(password), connectTimeoutMs_(connectTimeoutMs) {}

bool WiFiNetwork::connect() {
    // Fast fail on an unconfigured SSID instead of burning radio time.
    if (ssid_ == nullptr || ssid_[0] == '\0') return false;
    if (isConnected()) return true;

    switch (WiFi.begin(ssid_, password_)) {
        case WL_CONNECTED:
            enableLowPowerMode();
            return true;
        case WL_NO_MODULE:
            // Radio missing or firmware handshake failed: nothing to poll.
            return false;
        default:
            break;
    }

    const unsigned long deadline = millis() + connectTimeoutMs_;
    int status = WiFi.status();
    while ((long)(deadline - millis()) > 0) {
        if (status == WL_CONNECTED) {
            enableLowPowerMode();
            return true;
        }
        // Wrong password / rejected credentials: retrying is pointless.
        if (status == WL_CONNECT_FAILED) {
            WiFi.disconnect();
            return false;
        }
        // Module dropped off the bus (brownout, firmware crash).
        if (status == WL_NO_MODULE) break;
        delay(100);
        status = WiFi.status();
    }

    WiFi.disconnect(); // leave the radio in a clean idle state
    return false;
}

bool WiFiNetwork::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiNetwork::disconnect() {
    WiFi.end(); // disconnect + stop + powers the NINA module down
}

int WiFiNetwork::getSignalQuality() const {
    if (!isConnected()) return -1;
    const int32_t rssi = WiFi.RSSI(); // ~0 dBm (excellent) to ~-100 dBm
    if (rssi >= 0) return -1;         // RSSI unavailable
    long quality = 2 * ((long)rssi + 100);
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;
    return (int)quality;
}

void WiFiNetwork::enableLowPowerMode() const {
    // DTIM power save on the NINA module: association kept, far lower
    // idle current. Must be re-enabled after each WiFi re-initialization.
    WiFi.lowPowerMode();
}
