#include "hal/implementations/ethernet_network.h"

#if ETHERNET_ENABLED

#include <cstring>

EthernetNetwork::EthernetNetwork(const uint8_t mac[6], uint32_t dhcpTimeoutMs)
    : dhcpTimeoutMs_(dhcpTimeoutMs) {
    memcpy(mac_, mac, sizeof(mac_));
}

bool EthernetNetwork::connect() {
    if (isConnected()) return true;

    // No shield present: fail fast (no DHCP retry loop).
    if (Ethernet.hardwareStatus() == EthernetNoHardware) return false;

    // Ethernet.begin(mac, timeout) returns 0 on DHCP failure.
    if (Ethernet.begin(mac_, dhcpTimeoutMs_) == 0) {
        configured_ = false;
        return false;
    }
    configured_ = true;
    return true;
}

bool EthernetNetwork::isConnected() const {
    return configured_ && Ethernet.linkStatus() == LinkON;
}

void EthernetNetwork::disconnect() {
    client_.stop();
    Ethernet.maintain();
}

#endif // ETHERNET_ENABLED
