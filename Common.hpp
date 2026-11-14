// Common.hpp - shared types for the ALF-EWG project (Unit I: inline functions)
#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace DisasterCore {

enum class AlertLevel : int { Normal = 0, Watch = 1, Warning = 2, Evacuate = 3 };

inline const char* toString(AlertLevel l) {
    switch (l) {
        case AlertLevel::Normal:   return "NORMAL";
        case AlertLevel::Watch:    return "WATCH";
        case AlertLevel::Warning:  return "WARNING";
        case AlertLevel::Evacuate: return "EVACUATE";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, AlertLevel l) { return os << toString(l); }

// Hazard index is in [0,100]
inline AlertLevel levelFromHazard(double h) {
    if (h >= 65.0) return AlertLevel::Evacuate;
    if (h >= 40.0) return AlertLevel::Warning;
    if (h >= 20.0) return AlertLevel::Watch;
    return AlertLevel::Normal;
}

// FNV-1a 32-bit checksum (chainable through the seed argument)
inline std::uint32_t checksum32(const void* data, std::size_t n,
                                std::uint32_t seed = 2166136261u) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint32_t h = seed;
    for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

}  // namespace DisasterCore
