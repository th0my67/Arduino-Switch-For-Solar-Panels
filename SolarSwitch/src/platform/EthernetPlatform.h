#pragma once
#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>

#include "../hal/INetwork.h"
#include "../hal/IStatusLed.h"

namespace solar {

// MKR ETH Shield (W5500) adapter, DHCP-based.
class EthernetNetwork : public INetwork {
 public:
  EthernetNetwork(const uint8_t* mac, IStatusLed& led) : mac_(mac), led_(led) {}

  bool begin() override {
    while (Ethernet.linkStatus() == LinkOFF) led_.signal(LedEvent::LinkError);
    while (Ethernet.begin(const_cast<uint8_t*>(mac_)) == 0) {
      led_.signal(LedEvent::DhcpError);
    }
    return true;
  }

  bool ensureUp() override {
    const int maintain = Ethernet.maintain();  // renew the DHCP lease
    if (maintain == 1 || maintain == 3) return begin();  // renew/rebind failed
    if (Ethernet.linkStatus() == LinkOFF) return begin();
    return true;
  }

 private:
  const uint8_t* mac_;
  IStatusLed& led_;
};

}  // namespace solar
