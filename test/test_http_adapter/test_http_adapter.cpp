#include <unity.h>
#include <Arduino.h>
#include <string>
#include "hal/implementations/arduino_http_client_adapter.h"
#include "hal/interfaces/i_network_interface.h"

// Fake raw Client: serves a canned response byte-per-byte.
class FakeRawClient : public Client {
public:
    void setResponse(const std::string& r) { response_ = r; pos_ = 0; }
    void setConnectResult(int result) { connectResult_ = result; }
    void setNeverConnects(bool v) { neverConnects_ = v; }
    bool stopped = false;
    std::string lastRequest;
    uint16_t lastPort = 0;

    int connect(IPAddress, uint16_t port) override {
        lastPort = port;
        return connectResult_;
    }
    int connect(const char*, uint16_t port) override {
        lastPort = port;
        return connectResult_;
    }
    size_t write(uint8_t b) override {
        lastRequest += (char)b;
        return 1;
    }
    int available() override {
        return (neverConnects_ || pos_ >= response_.size()) ? 0
                                                            : (int)(response_.size() - pos_);
    }
    int read() override {
        if (neverConnects_ || pos_ >= response_.size()) return -1;
        return (uint8_t)response_[pos_++];
    }
    void stop() override { stopped = true; }
    uint8_t connected() override {
        return (neverConnects_ || pos_ < response_.size()) ? 1 : 0;
    }
    operator bool() override { return true; }

private:
    std::string response_;
    size_t pos_ = 0;
    int connectResult_ = 1;
    bool neverConnects_ = false;
};

class FakeNetwork : public INetworkInterface {
public:
    FakeRawClient rawClient;
    bool networkConnected = true;
    bool connectResult = true;
    int connectCalls = 0;

    bool connect() override {
        ++connectCalls;
        networkConnected = connectResult;
        return connectResult;
    }
    bool isConnected() const override { return networkConnected; }
    void disconnect() override { networkConnected = false; }
    Client* createClient() override { return &rawClient; }
    const char* getConnectionType() const override { return "Fake"; }
    int getSignalQuality() const override { return -1; }
};

void setUp(void) {}
void tearDown(void) {}

static const char kUrl[] = "http://192.168.1.161:5000/solar_api/v1/GetInverterRealtimeData.cgi";

void test_200_with_body(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_STRING("hello", r.unwrap().c_str());
    TEST_ASSERT_TRUE(net.rawClient.stopped); // connection always closed
    // The request must hit the right path with GET.
    TEST_ASSERT_NOT_NULL(strstr(net.rawClient.lastRequest.c_str(), "GET /solar_api/v1/GetInverterRealtimeData.cgi"));
    TEST_ASSERT_EQUAL_UINT16(5000, net.rawClient.lastPort);
}

void test_404_maps_to_not_found(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT((int)HttpError::NotFound, (int)r.unwrapErr());
}

void test_500_maps_to_server_error(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::ServerError, (int)r.unwrapErr());
}

void test_400_maps_to_client_error(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::ClientError, (int)r.unwrapErr());
}

void test_connection_refused_maps_to_network_failure(void) {
    FakeNetwork net;
    net.rawClient.setConnectResult(0); // connect() returns 0 -> refused
    ArduinoHttpClientAdapter adapter(net, 1000);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT((int)HttpError::NetworkFailure, (int)r.unwrapErr());
}

void test_invalid_url_maps_to_invalid_response(void) {
    FakeNetwork net;
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get("not-a-url", 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::InvalidResponse, (int)r.unwrapErr());
    TEST_ASSERT_EQUAL_INT(0, net.connectCalls); // no network touched
}

void test_offline_network_autoconnects(void) {
    FakeNetwork net;
    net.networkConnected = false;
    net.connectResult = true;
    net.rawClient.setResponse("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(1, net.connectCalls);
}

void test_offline_network_connect_fails(void) {
    FakeNetwork net;
    net.networkConnected = false;
    net.connectResult = false;
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::NetworkFailure, (int)r.unwrapErr());
}

void test_body_over_cap_is_rejected(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
    ArduinoHttpClientAdapter adapter(net, 16); // tiny cap on purpose
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::InvalidResponse, (int)r.unwrapErr());
    TEST_ASSERT_TRUE(net.rawClient.stopped);
}

void test_no_content_length_reads_until_close(void) {
    FakeNetwork net;
    net.rawClient.setResponse("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n{\"a\":1}");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_STRING("{\"a\":1}", r.unwrap().c_str());
}

void test_garbage_status_line_is_invalid_response(void) {
    FakeNetwork net;
    net.rawClient.setResponse("GARBAGE NOT HTTP\r\n\r\n");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 1000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::InvalidResponse, (int)r.unwrapErr());
}

void test_timeout_waiting_for_status(void) {
    FakeNetwork net;
    net.rawClient.setNeverConnects(true); // never a single byte
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 3000);
    TEST_ASSERT_EQUAL_INT((int)HttpError::Timeout, (int)r.unwrapErr());
}

void test_truncated_body_is_rejected(void) {
    FakeNetwork net;
    // Announces 10 bytes, closes after 4.
    net.rawClient.setResponse("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabcd");
    ArduinoHttpClientAdapter adapter(net, 4096);
    auto r = adapter.get(kUrl, 200);
    TEST_ASSERT_EQUAL_INT((int)HttpError::InvalidResponse, (int)r.unwrapErr());
}

void test_status_classification(void) {
    TEST_ASSERT_TRUE(ArduinoHttpClientAdapter::isStatusSuccess(200));
    TEST_ASSERT_TRUE(ArduinoHttpClientAdapter::isStatusSuccess(204));
    TEST_ASSERT_TRUE(ArduinoHttpClientAdapter::isStatusSuccess(301));
    TEST_ASSERT_FALSE(ArduinoHttpClientAdapter::isStatusSuccess(199));
    TEST_ASSERT_FALSE(ArduinoHttpClientAdapter::isStatusSuccess(400));
    TEST_ASSERT_EQUAL_INT((int)HttpError::ServerError, (int)ArduinoHttpClientAdapter::classifyStatus(503));
    TEST_ASSERT_EQUAL_INT((int)HttpError::NotFound, (int)ArduinoHttpClientAdapter::classifyStatus(404));
    TEST_ASSERT_EQUAL_INT((int)HttpError::ClientError, (int)ArduinoHttpClientAdapter::classifyStatus(401));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_200_with_body);
    RUN_TEST(test_404_maps_to_not_found);
    RUN_TEST(test_500_maps_to_server_error);
    RUN_TEST(test_400_maps_to_client_error);
    RUN_TEST(test_connection_refused_maps_to_network_failure);
    RUN_TEST(test_invalid_url_maps_to_invalid_response);
    RUN_TEST(test_offline_network_autoconnects);
    RUN_TEST(test_offline_network_connect_fails);
    RUN_TEST(test_body_over_cap_is_rejected);
    RUN_TEST(test_no_content_length_reads_until_close);
    RUN_TEST(test_garbage_status_line_is_invalid_response);
    RUN_TEST(test_timeout_waiting_for_status);
    RUN_TEST(test_truncated_body_is_rejected);
    RUN_TEST(test_status_classification);
    return UNITY_END();
}
