// Nodes.hpp - Unit III: abstract base, virtual inheritance (diamond), containership,
//             runtime polymorphism
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include "Common.hpp"
#include "Exceptions.hpp"

namespace DisasterCore {
namespace Sensors {

// ---- independent object that is CONTAINED (has-a) in every sensor node ----
class PowerSubsystem {
    double capacityWh_, chargeWh_;
public:
    explicit PowerSubsystem(double capacityWh = 100.0) : capacityWh_(capacityWh), chargeWh_(capacityWh) {}
    bool draw(double wh) {
        if (chargeWh_ < wh) { chargeWh_ = 0; return false; }
        chargeWh_ -= wh; return true;
    }
    void   charge(double wh)  { chargeWh_ = std::min(capacityWh_, chargeWh_ + wh); }
    double percent()    const { return 100.0 * chargeWh_ / capacityWh_; }
    bool   isLow()      const { return percent() < 15.0; }
};

// ---- ROOT of the diamond ----
class DisasterTelemetryDevice {
    int         deviceId_;
    std::string firmware_;
public:
    explicit DisasterTelemetryDevice(int id = 0, std::string fw = "fw-1.0")
        : deviceId_(id), firmware_(std::move(fw)) {}
    virtual ~DisasterTelemetryDevice() = default;
    int deviceId() const { return deviceId_; }
    const std::string& firmware() const { return firmware_; }
};

// ---- abstract base class (classification: "is-a" device) ----
class BaseSensorNode : public virtual DisasterTelemetryDevice {
    int           stationId_;
    std::string   label_;
    PowerSubsystem power_;                 // containership: "has-a" power subsystem
    double        baseline_, drift_, noise_;
    unsigned      cycles_;
    double        last_;
    std::mt19937  rng_;
    bool          corruptNext_, failNext_;

protected:
    PowerSubsystem& power() { return power_; }

    // Simulated acquisition: drains power, may fail, verifies packet checksum
    double acquire(double lo, double hi) {
        if (failNext_) {
            failNext_ = false;
            throw SensorFailureException(deviceId(), label_ + " hardware fault (no response)");
        }
        if (!power_.draw(0.8))
            throw SensorFailureException(deviceId(), label_ + " battery depleted");
        ++cycles_;
        std::uniform_real_distribution<double> jitter(-noise_, noise_);
        double v = baseline_ + drift_ * cycles_ + jitter(rng_);
        v = std::max(lo, std::min(hi, v));

        std::uint32_t sent = checksum32(&v, sizeof v);
        double wire = v;                                   // simulate the radio link
        if (corruptNext_) {
            corruptNext_ = false;
            unsigned char* p = reinterpret_cast<unsigned char*>(&wire);
            p[0] ^= 0xFF;
        }
        std::uint32_t got = checksum32(&wire, sizeof wire);
        if (sent != got) throw TelemetryChecksumMismatch(sent, got);
        last_ = v;
        return v;
    }

    bool checkThreshold(double warn, double hard) const {
        if (last_ > hard) throw ThresholdOverflowException(last_, hard);
        return last_ >= warn;
    }

public:
    BaseSensorNode(int stationId, std::string label, double baseline, double drift,
                   double noise, unsigned seed)
        : stationId_(stationId), label_(std::move(label)), power_(100.0),
          baseline_(baseline), drift_(drift), noise_(noise), cycles_(0), last_(0.0),
          rng_(seed), corruptNext_(false), failNext_(false) {}
    ~BaseSensorNode() override = default;

    // pure virtual interface
    virtual double      readTelemetry() = 0;
    virtual bool        evaluateThreshold() const = 0;
    virtual std::string typeName() const = 0;

    int  stationId() const { return stationId_; }
    double lastReading() const { return last_; }
    const PowerSubsystem& powerStatus() const { return power_; }
    void injectCorruption() { corruptNext_ = true; }
    void injectFailure()    { failNext_ = true; }
};

// ---- concrete single-purpose sensors ----
class SoilMoistureSensor : public BaseSensorNode {
public:
    SoilMoistureSensor(int id, int station, double base, double drift, unsigned seed)
        : DisasterTelemetryDevice(id, "SMS-1.4"),
          BaseSensorNode(station, "SoilMoisture", base, drift, 0.8, seed) {}
    double readTelemetry() override { return acquire(0.0, 100.0); }
    bool evaluateThreshold() const override { return checkThreshold(70.0, 98.0); }
    std::string typeName() const override { return "SoilMoistureSensor"; }
};

class SeismicGeophone : public BaseSensorNode {
public:
    SeismicGeophone(int id, int station, double base, double drift, unsigned seed)
        : DisasterTelemetryDevice(id, "GEO-2.2"),
          BaseSensorNode(station, "Geophone", base, drift, 0.5, seed) {}
    double readTelemetry() override { return acquire(0.0, 60.0); }
    bool evaluateThreshold() const override { return checkThreshold(20.0, 55.0); }
    std::string typeName() const override { return "SeismicGeophone"; }
};

class RiverSonarGauge : public BaseSensorNode {
public:
    RiverSonarGauge(int id, int station, double base, double drift, unsigned seed)
        : DisasterTelemetryDevice(id, "SNR-3.0"),
          BaseSensorNode(station, "RiverSonar", base, drift, 0.1, seed) {}
    double readTelemetry() override { return acquire(0.0, 15.0); }
    bool evaluateThreshold() const override { return checkThreshold(6.0, 14.0); }
    std::string typeName() const override { return "RiverSonarGauge"; }
};

// ---- intermediate classes: VIRTUAL inheritance from the root ----
class SolarPoweredNode : public virtual DisasterTelemetryDevice {
    double panelWatts_;
public:
    explicit SolarPoweredNode(double panelWatts = 20.0) : panelWatts_(panelWatts) {}
    double harvest(double sunlightFraction) const { return panelWatts_ * sunlightFraction * 0.5; }
    double panelWatts() const { return panelWatts_; }
};

class WirelessLoRaTransceiver : public virtual DisasterTelemetryDevice {
    double      freqMHz_;
    std::size_t sent_;
public:
    explicit WirelessLoRaTransceiver(double freqMHz = 865.0) : freqMHz_(freqMHz), sent_(0) {}
    bool transmit(double /*payload*/) { ++sent_; return true; }
    std::size_t packetsSent() const { return sent_; }
    double frequencyMHz() const { return freqMHz_; }
};

// ---- derived leaf: diamond resolved, single DisasterTelemetryDevice subobject ----
class AutonomousHybridProbe : public BaseSensorNode,
                              public SolarPoweredNode,
                              public WirelessLoRaTransceiver {
public:
    AutonomousHybridProbe(int id, int station, double base, double drift, unsigned seed)
        : DisasterTelemetryDevice(id, "AHP-2.0"),             // most-derived class builds the virtual base
          BaseSensorNode(station, "AcousticTiltmeter", base, drift, 0.4, seed),
          SolarPoweredNode(30.0),
          WirelessLoRaTransceiver(868.0) {}

    double readTelemetry() override {
        double v = acquire(0.0, 90.0);
        power().charge(harvest(0.6));      // solar mix-in
        transmit(v);                       // LoRa mix-in
        return v;
    }
    bool evaluateThreshold() const override { return checkThreshold(15.0, 80.0); }
    std::string typeName() const override { return "AutonomousHybridProbe"; }
};

}  // namespace Sensors
}  // namespace DisasterCore
