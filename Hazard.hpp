// Hazard.hpp - Unit I: namespaces, inline functions, overloading, call/return by reference
#pragma once
#include <algorithm>
#include <cstddef>
#include "Common.hpp"

namespace DisasterCore {
namespace Sensors {

struct GeoCoord       { double latitude{0.0}; double longitude{0.0}; };
struct RainfallSlope  { double rainfallMmHr; double slopeDeg; };
struct SoilVibration  { double saturationPct; double vibrationHz; };
struct TelemetryState { double sum{0.0}; double peak{0.0}; std::size_t samples{0}; };

// ---- inline helpers for real-time risk indexing -------------------------
inline double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
inline double weighted(double w1, double n1, double w2, double n2) {
    return 100.0 * (w1 * n1 + w2 * n2);
}

// ---- overloaded computeHazardIndex() -----------------------------------
inline double computeHazardIndex(const RainfallSlope& rs) {
    return weighted(0.6, clamp01(rs.rainfallMmHr / 100.0), 0.4, clamp01(rs.slopeDeg / 60.0));
}
inline double computeHazardIndex(const SoilVibration& sv) {
    return weighted(0.7, clamp01(sv.saturationPct / 100.0), 0.3, clamp01(sv.vibrationHz / 50.0));
}
inline double computeHazardIndex(double riverLevelM, double riseRateCmPerMin) {
    return weighted(0.5, clamp01(riverLevelM / 10.0), 0.5, clamp01(riseRateCmPerMin / 20.0));
}
inline double computeHazardIndex(const RainfallSlope& rs, const SoilVibration& sv) {
    double a = computeHazardIndex(rs), b = computeHazardIndex(sv);
    return 0.6 * std::max(a, b) + 0.4 * std::min(a, b);
}

// ---- call by reference: in-place geocoordinate transformation ----------
inline void translateCoordinate(GeoCoord& c, double dLat, double dLon) {
    c.latitude  = std::max(-90.0,  std::min(90.0,  c.latitude  + dLat));
    c.longitude = std::max(-180.0, std::min(180.0, c.longitude + dLon));
}

// ---- return by reference: enables chained accumulation -----------------
inline TelemetryState& accumulate(TelemetryState& s, double v) {
    s.sum += v;
    s.peak = std::max(s.peak, v);
    ++s.samples;
    return s;
}

}  // namespace Sensors
}  // namespace DisasterCore
