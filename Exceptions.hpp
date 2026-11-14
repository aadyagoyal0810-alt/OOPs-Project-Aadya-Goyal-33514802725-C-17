// Exceptions.hpp - custom exception hierarchy (Unit IV)
#pragma once
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

namespace DisasterCore {

class SensorFailureException : public std::exception {
    std::string msg_;
public:
    SensorFailureException(int deviceId, const std::string& reason)
        : msg_("[SensorFailureException] device #" + std::to_string(deviceId) + ": " + reason) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

class TelemetryChecksumMismatch : public std::exception {
    std::string msg_;
public:
    TelemetryChecksumMismatch(std::uint32_t expected, std::uint32_t actual) {
        std::ostringstream o;
        o << "[TelemetryChecksumMismatch] expected 0x" << std::hex << std::setw(8)
          << std::setfill('0') << expected << " but computed 0x" << std::setw(8) << actual;
        msg_ = o.str();
    }
    const char* what() const noexcept override { return msg_.c_str(); }
};

class ThresholdOverflowException : public std::exception {
    std::string msg_;
public:
    ThresholdOverflowException(double value, double hardLimit) {
        std::ostringstream o;
        o << "[ThresholdOverflowException] reading " << value
          << " exceeds physical hard limit " << hardLimit << " (sensor saturated?)";
        msg_ = o.str();
    }
    const char* what() const noexcept override { return msg_.c_str(); }
};

// Extra: used by the persistence layer
class PersistenceException : public std::exception {
    std::string msg_;
public:
    explicit PersistenceException(const std::string& m) : msg_("[PersistenceException] " + m) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

}  // namespace DisasterCore
