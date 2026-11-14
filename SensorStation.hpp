// SensorStation.hpp - Unit II: abstraction, const-correctness, static members,
//                     friend class, operator overloading (+, >, <<, >>)
#pragma once
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "Common.hpp"
#include "Hazard.hpp"
#include "TelemetryBuffer.hpp"

namespace DisasterCore {
namespace Sensors {

class SensorStation {
    int             id_;
    std::string     name_;
    GeoCoord        loc_;
    TelemetryBuffer buffer_;     // composition: station owns its buffer
    double          hazard_;
    AlertLevel      level_;

    static inline int        activeCount_  = 0;                  // network-wide station count
    static inline AlertLevel globalAlert_  = AlertLevel::Normal; // network-wide alert level

    friend class TelemetryAuditor;

public:
    SensorStation()
        : id_(0), name_("unnamed"), loc_(), buffer_(8), hazard_(0), level_(AlertLevel::Normal) { ++activeCount_; }

    SensorStation(int id, const std::string& name, GeoCoord loc, std::size_t bufferCapacity = 8)
        : id_(id), name_(name), loc_(loc), buffer_(bufferCapacity), hazard_(0), level_(AlertLevel::Normal) {
        ++activeCount_;
    }

    SensorStation(const SensorStation& o)
        : id_(o.id_), name_(o.name_), loc_(o.loc_), buffer_(o.buffer_), hazard_(o.hazard_), level_(o.level_) {
        ++activeCount_;
    }
    SensorStation& operator=(const SensorStation&) = default;   // buffer_ deep-copies itself
    ~SensorStation() { --activeCount_; }

    // ---- const-correct public interface ----
    int                 id()     const { return id_; }
    const std::string&  name()   const { return name_; }
    double              hazard() const { return hazard_; }
    AlertLevel          level()  const { return level_; }

    GeoCoord&       location()       { return loc_; }   // return by reference
    const GeoCoord& location() const { return loc_; }

    void recordReading(double v) { buffer_.push(v); }
    void setHazard(double h) {
        hazard_ = h;
        level_  = levelFromHazard(h);
        raiseGlobalAlert(level_);
    }

    // ---- static interface ----
    static int        activeCount()                  { return activeCount_; }
    static AlertLevel globalAlert()                  { return globalAlert_; }
    static void       raiseGlobalAlert(AlertLevel l) { if (l > globalAlert_) globalAlert_ = l; }
    static void       resetGlobalAlert()             { globalAlert_ = AlertLevel::Normal; }

    // ---- relational operator: prioritise higher hazard ----
    bool operator>(const SensorStation& o) const { return hazard_ > o.hazard_; }

    // ---- stream insertion / extraction: "id name lat lon hazard" ----
    friend std::ostream& operator<<(std::ostream& os, const SensorStation& s) {
        return os << s.id_ << ' ' << s.name_ << ' ' << s.loc_.latitude << ' '
                  << s.loc_.longitude << ' ' << s.hazard_;
    }
    friend std::istream& operator>>(std::istream& is, SensorStation& s) {
        int id; std::string name; double lat, lon, h;
        if (is >> id >> name >> lat >> lon >> h) {
            s.id_ = id; s.name_ = name; s.loc_ = GeoCoord{lat, lon};
            s.hazard_ = h; s.level_ = levelFromHazard(h);
        }
        return is;
    }
};

// Friend class: inspects raw private metrics without any getters
class TelemetryAuditor {
public:
    void inspect(const SensorStation& s, std::ostream& os) const {
        const TelemetryBuffer& b = s.buffer_;
        os << "  [AUDIT] station " << s.id_ << " (" << s.name_ << ") raw ring buffer cap="
           << b.capacity_ << " n=" << b.count_ << " start=" << b.start_ << " raw=[ ";
        for (std::size_t i = 0; i < b.capacity_; ++i) os << b.data_[i] << ' ';
        os << "]\n";
    }
};

// Regional zone vector produced by binary operator+
class ZoneVector {
    std::string                 name_;
    std::vector<SensorStation>  stations_;
public:
    explicit ZoneVector(std::string name = "Zone") : name_(std::move(name)) {}
    ZoneVector& add(const SensorStation& s) { stations_.push_back(s); return *this; }
    void rename(const std::string& n) { name_ = n; }
    const std::string& name() const { return name_; }
    std::size_t size() const { return stations_.size(); }
    const std::vector<SensorStation>& stations() const { return stations_; }
    double averageHazard() const {
        if (stations_.empty()) return 0.0;
        double s = 0; for (const auto& st : stations_) s += st.hazard();
        return s / static_cast<double>(stations_.size());
    }
    double peakHazard() const {
        double p = 0; for (const auto& st : stations_) p = std::max(p, st.hazard());
        return p;
    }
};

inline ZoneVector operator+(const SensorStation& a, const SensorStation& b) {
    ZoneVector z("Zone"); z.add(a).add(b); return z;
}
inline ZoneVector operator+(ZoneVector z, const SensorStation& s) { z.add(s); return z; }
inline ZoneVector operator+(ZoneVector a, const ZoneVector& b) {
    for (const auto& s : b.stations()) a.add(s);
    return a;
}
inline std::ostream& operator<<(std::ostream& os, const ZoneVector& z) {
    return os << "ZoneVector{" << z.name() << ", stations=" << z.size()
              << ", avgHazard=" << z.averageHazard() << ", peakHazard=" << z.peakHazard() << "}";
}

}  // namespace Sensors
}  // namespace DisasterCore
