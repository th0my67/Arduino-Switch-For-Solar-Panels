#include <unity.h>
#include "utils/result.h"

enum class FakeError { Alpha, Beta };

void setUp(void) {}
void tearDown(void) {}

void test_ok_holds_value(void) {
    Result<int, FakeError> r = Result<int, FakeError>::ok(42);
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_FALSE(r.isError());
    TEST_ASSERT_EQUAL_INT(42, r.unwrap());
}

void test_err_holds_error(void) {
    Result<int, FakeError> r = Result<int, FakeError>::err(FakeError::Beta);
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_FALSE(r.isOk());
    TEST_ASSERT_EQUAL_INT((int)FakeError::Beta, (int)r.unwrapErr());
}

void test_implicit_value_ctor(void) {
    Result<int, FakeError> r = 7;
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(7, r.unwrap());
}

void test_implicit_error_ctor(void) {
    Result<int, FakeError> r = FakeError::Alpha;
    TEST_ASSERT_TRUE(r.isError());
}

void test_unwrap_or_returns_value_on_ok(void) {
    Result<int, FakeError> r = Result<int, FakeError>::ok(9);
    TEST_ASSERT_EQUAL_INT(9, r.unwrapOr(0));
}

void test_unwrap_or_returns_default_on_err(void) {
    Result<int, FakeError> r = Result<int, FakeError>::err(FakeError::Alpha);
    TEST_ASSERT_EQUAL_INT(-1, r.unwrapOr(-1));
}

void test_same_value_and_error_type(void) {
    // Result<int,int>: the ok()/err() factories avoid ctor ambiguity.
    Result<int, int> okCase = Result<int, int>::ok(3);
    Result<int, int> errCase = Result<int, int>::err(4);
    TEST_ASSERT_TRUE(okCase.isOk());
    TEST_ASSERT_EQUAL_INT(3, okCase.unwrap());
    TEST_ASSERT_TRUE(errCase.isError());
    TEST_ASSERT_EQUAL_INT(4, errCase.unwrapErr());
}

void test_enum_payload(void) {
    Result<int, FakeError> err = FakeError::Beta;
    TEST_ASSERT_EQUAL_INT((int)FakeError::Beta, (int)err.unwrapErr());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ok_holds_value);
    RUN_TEST(test_err_holds_error);
    RUN_TEST(test_implicit_value_ctor);
    RUN_TEST(test_implicit_error_ctor);
    RUN_TEST(test_unwrap_or_returns_value_on_ok);
    RUN_TEST(test_unwrap_or_returns_default_on_err);
    RUN_TEST(test_same_value_and_error_type);
    RUN_TEST(test_enum_payload);
    return UNITY_END();
}
