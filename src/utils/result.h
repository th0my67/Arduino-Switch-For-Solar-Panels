#ifndef RESULT_H
#define RESULT_H

#include <optional>

template<typename T, typename E>
class Result {
public:
    // Success constructor
    Result(T value) : value_(value), error_(std::nullopt) {}
    
    // Error constructor  
    Result(E error) : value_(std::nullopt), error_(error) {}
    
    // Check if operation succeeded
    bool isOk() const { return value_.has_value(); }
    bool isError() const { return error_.has_value(); }
    
    // Get value (only call if isOk() == true)
    T unwrap() const { return value_.value(); }
    
    // Get error (only call if isError() == true)
    E unwrapErr() const { return error_.value(); }
    
    // Safe value access with default
    T unwrapOr(const T& defaultValue) const {
        return isOk() ? unwrap() : defaultValue;
    }

private:
    std::optional<T> value_;
    std::optional<E> error_;
};

#endif