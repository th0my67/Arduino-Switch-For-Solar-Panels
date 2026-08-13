#pragma once
#include <Arduino.h>
#include <Client.h>
#include <string.h>

#include "../hal/IHttpClient.h"

namespace solar {

// IByteReader over an Arduino Client with an inactivity timeout.
class ClientByteReader : public IByteReader {
 public:
  void attach(Client* client, uint32_t timeoutMs) {
    client_ = client;
    timeoutMs_ = timeoutMs;
  }

  int read() override {
    if (!client_) return -1;
    const uint32_t start = millis();
    while (true) {
      if (client_->available()) return client_->read();
      if (!client_->connected()) return -1;
      if (millis() - start >= timeoutMs_) return -1;
      delay(1);
    }
  }

 private:
  Client* client_ = nullptr;
  uint32_t timeoutMs_ = 5000;
};

// HTTP GET over any Arduino Client (EthernetClient, WiFiClient,
// WiFiSSLClient...). Board-agnostic: give it a plain client and, when the
// hardware supports it, a TLS client. Requests are sent as HTTP/1.0 on
// purpose: it forbids chunked transfer encoding, which would corrupt the
// streaming JSON scan (the body is read until the server closes).
class HttpOverClient : public IHttpClient {
 public:
  HttpOverClient(Client& plain, Client* tls) : plain_(&plain), tls_(tls) {}

  IByteReader* get(const char* host, uint16_t port, const char* pathAndQuery,
                   bool tls, const char* extraHeaderLines) override {
    close();
    Client* client = tls ? tls_ : plain_;
    if (!client) return nullptr;  // TLS requested but unavailable on this board
    if (!client->connect(host, port)) return nullptr;
    current_ = client;

    client->print("GET ");
    client->print(pathAndQuery);
    client->print(" HTTP/1.0\r\nHost: ");
    client->print(host);
    client->print("\r\n");
    if (extraHeaderLines) client->print(extraHeaderLines);
    client->print("Connection: close\r\n\r\n");

    reader_.attach(client, kTimeoutMs);
    if (!checkStatusAndSkipHeaders()) {
      close();
      return nullptr;
    }
    return &reader_;
  }

  void close() override {
    if (current_) {
      current_->stop();
      current_ = nullptr;
    }
  }

 private:
  static const uint32_t kTimeoutMs = 8000;

  bool checkStatusAndSkipHeaders() {
    // Status line, e.g. "HTTP/1.1 200 OK".
    char line[16];
    uint8_t n = 0;
    int c;
    while ((c = reader_.read()) >= 0 && c != '\n') {
      if (n < sizeof(line) - 1) line[n++] = (char)c;
    }
    if (c < 0) return false;
    line[n] = '\0';
    if (strstr(line, " 200") == nullptr) return false;

    // Headers end at the first empty line.
    uint8_t newlines = 1;
    while (newlines < 2) {
      c = reader_.read();
      if (c < 0) return false;
      if (c == '\n') {
        newlines++;
      } else if (c != '\r') {
        newlines = 0;
      }
    }
    return true;
  }

  Client* plain_;
  Client* tls_;
  Client* current_ = nullptr;
  ClientByteReader reader_;
};

}  // namespace solar
