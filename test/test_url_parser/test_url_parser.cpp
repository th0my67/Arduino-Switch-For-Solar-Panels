#include <unity.h>
#include "utils/url_parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_valid_ip_with_port_path_and_query(void) {
    const char* url = "http://192.168.1.161:5000/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceId=1";
    const UrlComponents c = parseUrl(url);
    TEST_ASSERT_TRUE(c.isValid);
    TEST_ASSERT_TRUE(c.isIp);
    TEST_ASSERT_EQUAL_STRING("192.168.1.161", c.host.c_str());
    TEST_ASSERT_EQUAL_INT(5000, c.port);
    TEST_ASSERT_EQUAL_STRING("/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceId=1", c.path.c_str());
}

void test_ip_default_port_and_default_path(void) {
    const UrlComponents c = parseUrl("http://10.0.0.5");
    TEST_ASSERT_TRUE(c.isValid);
    TEST_ASSERT_TRUE(c.isIp);
    TEST_ASSERT_EQUAL_INT(80, c.port);
    TEST_ASSERT_EQUAL_STRING("/", c.path.c_str());
}

void test_hostname_is_accepted(void) {
    const UrlComponents c = parseUrl("http://inverter.local/solar/api");
    TEST_ASSERT_TRUE(c.isValid);
    TEST_ASSERT_FALSE(c.isIp);
    TEST_ASSERT_EQUAL_STRING("inverter.local", c.host.c_str());
    TEST_ASSERT_EQUAL_INT(80, c.port);
    TEST_ASSERT_EQUAL_STRING("/solar/api", c.path.c_str());
}

void test_max_port_value(void) {
    const UrlComponents c = parseUrl("http://192.168.1.161:65535/");
    TEST_ASSERT_TRUE(c.isValid);
    TEST_ASSERT_EQUAL_INT(65535, c.port);
}

void test_https_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("https://192.168.1.161/api").isValid);
}

void test_null_and_empty_are_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl(nullptr).isValid);
    TEST_ASSERT_FALSE(parseUrl("").isValid);
}

void test_scheme_only_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("http://").isValid);
    TEST_ASSERT_FALSE(parseUrl("http://:80/").isValid);
}

void test_no_scheme_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("192.168.1.161/api").isValid);
    TEST_ASSERT_FALSE(parseUrl("ftp://192.168.1.161/api").isValid);
}

void test_port_zero_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161:0/").isValid);
}

void test_port_overflow_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161:65536/").isValid);
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161:99999/").isValid);
}

void test_port_with_trailing_garbage_is_rejected(void) {
    // String::toInt would silently accept the numeric prefix; the parser must not.
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161:80abc/").isValid);
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161: 80/").isValid);
}

void test_empty_port_is_rejected(void) {
    TEST_ASSERT_FALSE(parseUrl("http://192.168.1.161:/").isValid);
}

void test_invalid_ip_becomes_hostname_or_is_rejected(void) {
    // Not a valid IP: "999" octet. Digits and dots are legal DNS characters,
    // so it parses as a hostname (isIp=false); DNS resolution then fails
    // cleanly at runtime. A space, however, is never legal.
    const UrlComponents numeric = parseUrl("http://192.168.1.999:80/");
    TEST_ASSERT_TRUE(numeric.isValid);
    TEST_ASSERT_FALSE(numeric.isIp);
    TEST_ASSERT_EQUAL_STRING("192.168.1.999", numeric.host.c_str());
    TEST_ASSERT_FALSE(parseUrl("http://19 2.168.1.1/").isValid);
    TEST_ASSERT_FALSE(parseUrl("http://host_name with space/").isValid);
}

void test_credentials_are_rejected(void) {
    // "user:pass@host" would parse as host "user" + invalid port.
    TEST_ASSERT_FALSE(parseUrl("http://user:pass@192.168.1.161/").isValid);
}

void test_overlong_url_is_rejected(void) {
    char big[300];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    // "http://" + 292 chars
    char url[320];
    snprintf(url, sizeof(url), "http://%s/", big);
    TEST_ASSERT_FALSE(parseUrl(url).isValid);
}

void test_userinfo_with_at_sign_only(void) {
    TEST_ASSERT_FALSE(parseUrl("http://@host/").isValid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_ip_with_port_path_and_query);
    RUN_TEST(test_ip_default_port_and_default_path);
    RUN_TEST(test_hostname_is_accepted);
    RUN_TEST(test_max_port_value);
    RUN_TEST(test_https_is_rejected);
    RUN_TEST(test_null_and_empty_are_rejected);
    RUN_TEST(test_scheme_only_is_rejected);
    RUN_TEST(test_no_scheme_is_rejected);
    RUN_TEST(test_port_zero_is_rejected);
    RUN_TEST(test_port_overflow_is_rejected);
    RUN_TEST(test_port_with_trailing_garbage_is_rejected);
    RUN_TEST(test_empty_port_is_rejected);
    RUN_TEST(test_invalid_ip_becomes_hostname_or_is_rejected);
    RUN_TEST(test_credentials_are_rejected);
    RUN_TEST(test_overlong_url_is_rejected);
    RUN_TEST(test_userinfo_with_at_sign_only);
    return UNITY_END();
}
