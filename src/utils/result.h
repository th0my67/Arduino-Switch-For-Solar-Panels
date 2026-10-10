#ifndef RESULT_H
#define RESULT_H

#include <type_traits>
#include <utility>

// Rust-like Result<T, E>: holds either a value or an error, never both.
// unwrap()/unwrapErr() must only be called after checking isOk()/isError().
// For types T and E that are identical, use the ok()/err() factories: the
// implicit single-argument constructors are then ambiguous (the value
// constructor is disabled by SFINAE in that case).
//
// Kept C++11-compatible: the Arduino framework compiles with -std=gnu++11,
// and std::optional is both C++17-only and heavier than this layout.
template <typename T, typename E>
class Result {
public:
    template <typename U,
              typename = typename std::enable_if<!std::is_same<U, E>::value>::type>
    Result(U value) : value_(std::move(value)), hasError_(false) {}

    Result(E error) : error_(std::move(error)), hasError_(true) {}

    static Result ok(T value) {
        Result r;
        r.value_ = std::move(value);
        r.hasError_ = false;
        return r;
    }

    static Result err(E error) {
        Result r;
        r.error_ = std::move(error);
        r.hasError_ = true;
        return r;
    }

    bool isOk() const { return !hasError_; }
    bool isError() const { return hasError_; }

    T unwrap() const { return value_; }
    E unwrapErr() const { return error_; }

    T unwrapOr(const T& defaultValue) const {
        return isOk() ? value_ : defaultValue;
    }

private:
    Result() : value_(), error_(), hasError_(false) {}

    T value_;
    E error_;
    bool hasError_;
};

#endif
