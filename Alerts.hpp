// Alerts.hpp - namespace DisasterCore::Alerts: sirens, dispatcher, functors, manipulators
#pragma once
#include <algorithm>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include "Common.hpp"
#include "SensorStation.hpp"

namespace DisasterCore {
namespace Alerts {

using Sensors::SensorStation;

class EarlyWarningSiren {
    std::string region_;
    int         volumeDb_;
    AlertLevel  state_;
    int         activations_;
    static inline int totalActivations_ = 0;       // static: across all sirens
public:
    explicit EarlyWarningSiren(std::string region, int volumeDb = 110)
        : region_(std::move(region)), volumeDb_(volumeDb), state_(AlertLevel::Normal), activations_(0) {}
    void trigger(AlertLevel l) {
        if (l == state_) return;
        state_ = l;
        if (l >= AlertLevel::Warning) { ++activations_; ++totalActivations_; }
    }
    void silence() { state_ = AlertLevel::Normal; }
    AlertLevel state() const { return state_; }
    bool active() const { return state_ >= AlertLevel::Warning; }
    const std::string& region() const { return region_; }
    int volumeDb() const { return volumeDb_; }
    int activations() const { return activations_; }
    static int totalActivations() { return totalActivations_; }
};

class AlertDispatcher {
    std::map<int, EarlyWarningSiren> sirens_;      // station id -> regional siren
public:
    void registerSiren(int stationId, const EarlyWarningSiren& s) { sirens_.emplace(stationId, s); }
    void dispatch(const SensorStation& st) {
        auto it = sirens_.find(st.id());
        if (it != sirens_.end()) it->second.trigger(st.level());
    }
    const EarlyWarningSiren* siren(int stationId) const {
        auto it = sirens_.find(stationId);
        return it == sirens_.end() ? nullptr : &it->second;
    }
};

// ---- custom functors (priority-based evacuation routing) ----
struct EvacuationPriority {                       // used by std::sort: highest hazard first
    bool operator()(const SensorStation& a, const SensorStation& b) const {
        if (a > b) return true;
        if (b > a) return false;
        return a.id() < b.id();
    }
};
struct SirenActivator {                           // used by std::for_each
    AlertDispatcher& dispatcher;
    void operator()(const SensorStation& s) const { dispatcher.dispatch(s); }
};
struct CountAtOrAbove {                           // used by std::count_if
    AlertLevel minLevel;
    bool operator()(const SensorStation& s) const { return s.level() >= minLevel; }
};

// ---- custom stream manipulators ----
struct field { int width; };                      // parameterised manipulator
inline std::ostream& operator<<(std::ostream& os, field f) { return os << std::left << std::setw(f.width); }

inline std::ostream& tableRule(std::ostream& os) {
    return os << '+' << std::string(6, '-') << '+' << std::string(20, '-') << '+'
              << std::string(9, '-') << '+' << std::string(11, '-') << '+'
              << std::string(11, '-') << "+\n";
}
inline std::ostream& alertHeader(std::ostream& os) {
    return os << "| " << field{4} << "ID" << " | " << field{18} << "Station" << " | "
              << field{7} << "Hazard" << " | " << field{9} << "Level" << " | "
              << field{9} << "Siren" << " |\n";
}

inline std::string fixed1(double v) {
    std::ostringstream o; o << std::fixed << std::setprecision(1) << v; return o.str();
}

inline void printDispatchTable(std::ostream& os, const std::vector<SensorStation>& sorted,
                               const AlertDispatcher& d) {
    os << tableRule << alertHeader << tableRule;
    for (const auto& s : sorted) {
        const EarlyWarningSiren* sr = d.siren(s.id());
        os << "| " << field{4} << s.id() << " | " << field{18} << s.name() << " | "
           << field{7} << fixed1(s.hazard()) << " | " << field{9} << s.level() << " | "
           << field{9} << (sr && sr->active() ? "SOUNDING" : "idle") << " |\n";
    }
    os << tableRule;
}

}  // namespace Alerts
}  // namespace DisasterCore
