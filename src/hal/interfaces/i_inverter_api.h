#ifndef I_INVERTER_API_H
#define I_INVERTER_API_H

#include "utils/result.h"
#include "hal/interfaces/i_network_interface.h"
#include <memory>

enum class InverterError {
    NetworkFailure,
    AuthenticationFailure,
    InvalidResponse,
    RateLimited,
    ServiceUnavailable
};

enum class InverterStatus {
    Offline,
    Idle,
    Producing,
    Error
};

class IInverterAPI {
public:
    virtual ~IInverterAPI() = default;
    virtual Result<float, InverterError> getCurrentPower() = 0;
    virtual Result<InverterStatus, InverterError> getStatus() = 0;
    virtual const char* getBrandName() const = 0;
};
#endif