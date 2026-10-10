#ifndef ETHERNET_NETWORK_H
#define ETHERNET_NETWORK_H

#if ETHERNET_ENABLED

#include "hal/interfaces/i_network_interface.h"
#include <Ethernet.h>

// Ethernet shield implementation (MKR ETH Shield on SPI). Only compiled
// when ETHERNET_ENABLED is set (see platformio.ini).
class EthernetNetwork : public INetworkInterface {
public:
    explicit EthernetNetwork(const uint8_t mac[6], uint32_t dhcpTimeoutMs = 10000);

    bool connect() override;      // DHCP lease, bounded by dhcpTimeoutMs
    bool isConnected() const override;
    void disconnect() override;   // stops the shield (MAC remains configured)
    Client* createClient() override { return &client_; }
    const char* getConnectionType() const override { return "Ethernet"; }
    int getSignalQuality() const override { return -1; }

private:
    uint8_t mac_[6];
    uint32_t dhcpTimeoutMs_;
    bool configured_ = false;
    EthernetClient client_;
};

#endif // ETHERNET_ENABLED
#endif
