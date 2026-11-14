// main.cpp - ALF-EWG: Autonomous Landslide & Flash-Flood Early Warning Grid
// CIC-211 Object-Oriented Programming Using C++  (Project Option 1)
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Alerts.hpp"
#include "Common.hpp"
#include "Exceptions.hpp"
#include "Hazard.hpp"
#include "IncidentLogger.hpp"
#include "Nodes.hpp"
#include "SensorStation.hpp"
#include "TelemetryBuffer.hpp"

using namespace DisasterCore;

namespace {

void banner(const std::string& t) { std::cout << "\n==== " << t << " ====\n"; }

// RAII owner of the polymorphic fleet: deletes through BaseSensorNode* (virtual dtor chain)
struct FleetGuard {
    std::vector<Sensors::BaseSensorNode*> nodes;
    FleetGuard() = default;
    FleetGuard(const FleetGuard&) = delete;
    FleetGuard& operator=(const FleetGuard&) = delete;
    ~FleetGuard() { for (auto* n : nodes) delete n; }
};

struct StationSpec {
    int id; const char* name; double lat, lon;
    double rain, slope;
    double soil, soilD, geo, geoD, river, riverD, tilt, tiltD;
};
const StationSpec kSpecs[] = {
    {1, "Kedarnath_Ridge", 30.73, 79.07, 70, 28, 55, 9, 8, 5, 3.0, 1.0, 6, 3.0},
    {2, "Alaknanda_Gorge", 30.40, 79.32, 20, 18, 40, 3, 5, 2, 2.0, 0.4, 4, 1.0},
    {3, "Beas_Valley",     32.24, 77.19, 55, 22, 65, 6, 10, 4, 4.0, 1.2, 8, 2.5},
    {4, "Teesta_Basin",    27.33, 88.61, 10, 10, 30, 1, 3, 1, 1.5, 0.3, 2, 0.5}};

// ---------------------------------------------------------------- Unit I
void demoUnitI() {
    banner("UNIT I: namespaces, inline, overloading, call/return by reference, friend");
    Sensors::RainfallSlope rs{65.0, 32.0};
    Sensors::SoilVibration sv{88.0, 27.0};
    std::cout << "computeHazardIndex(rainfall+slope)      = " << Sensors::computeHazardIndex(rs) << "\n";
    std::cout << "computeHazardIndex(saturation+vibration) = " << Sensors::computeHazardIndex(sv) << "\n";
    std::cout << "computeHazardIndex(river level+rise)     = " << Sensors::computeHazardIndex(7.5, 12.0) << "\n";
    std::cout << "computeHazardIndex(combined)             = " << Sensors::computeHazardIndex(rs, sv) << "\n";

    Sensors::GeoCoord c{30.73, 79.07};
    Sensors::translateCoordinate(c, 0.01, -0.02);              // call by reference
    std::cout << "Translated coordinate: (" << c.latitude << ", " << c.longitude << ")\n";

    Sensors::TelemetryState st;                                // return by reference => chaining
    Sensors::accumulate(Sensors::accumulate(Sensors::accumulate(st, 41.0), 55.5), 62.2);
    std::cout << "Accumulated state: samples=" << st.samples << " sum=" << st.sum << " peak=" << st.peak << "\n";
}

// ---------------------------------------------------------------- Unit II
void demoUnitII() {
    banner("UNIT II: dynamic memory, deep copy, static members, operators");
    {
        Sensors::TelemetryBuffer a(4);
        for (double v : {1.0, 2.0, 3.0}) a.push(v);
        double raw[] = {10.0, 20.0, 30.0};
        Sensors::TelemetryBuffer dyn(raw, 3);                  // dynamic constructor
        Sensors::TelemetryBuffer b = a;                        // deep copy constructor
        b.at(0) = 999.0;                                       // return by reference
        std::cout << "a[0]=" << a.at(0) << " (unchanged), b[0]=" << b.at(0)
                  << ", dyn mean=" << dyn.mean() << "\n";
        std::cout << "Live buffers inside scope: " << Sensors::TelemetryBuffer::liveBuffers() << "\n";
    }
    std::cout << "Live buffers after scope  : " << Sensors::TelemetryBuffer::liveBuffers() << " (no leaks)\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(2);
    std::remove("incidents.dat");
    std::cout << "ALF-EWG :: Autonomous Landslide & Flash-Flood Early Warning Grid\n";

    try {
        demoUnitI();
        demoUnitII();

        {   // scope so that destructors run before the final leak report
            banner("UNIT III: polymorphic sensor sweep (std::vector<BaseSensorNode*>)");

            std::vector<Sensors::SensorStation> stations;
            stations.reserve(4);
            Alerts::AlertDispatcher dispatcher;
            FleetGuard fleet;
            std::map<int, const StationSpec*> specById;
            unsigned seed = 7;
            int nextId = 100;

            for (const auto& sp : kSpecs) {
                stations.emplace_back(sp.id, sp.name, Sensors::GeoCoord{sp.lat, sp.lon}, 3);
                dispatcher.registerSiren(sp.id, Alerts::EarlyWarningSiren(std::string(sp.name) + "_Siren"));
                specById[sp.id] = &sp;
                // order per station: soil(0) geophone(1) river(2) hybrid-tiltmeter(3)
                fleet.nodes.push_back(new Sensors::SoilMoistureSensor(nextId++, sp.id, sp.soil, sp.soilD, seed++));
                fleet.nodes.push_back(new Sensors::SeismicGeophone(nextId++, sp.id, sp.geo, sp.geoD, seed++));
                fleet.nodes.push_back(new Sensors::RiverSonarGauge(nextId++, sp.id, sp.river, sp.riverD, seed++));
                fleet.nodes.push_back(new Sensors::AutonomousHybridProbe(nextId++, sp.id, sp.tilt, sp.tiltD, seed++));
            }
            auto idx = [](int station, int kind) { return static_cast<std::size_t>((station - 1) * 4 + kind); };

            // --- diamond problem: only ONE DisasterTelemetryDevice subobject ---
            {
                Sensors::AutonomousHybridProbe probe(900, 0, 5, 1, 1);
                const Sensors::DisasterTelemetryDevice* viaSolar =
                    static_cast<const Sensors::SolarPoweredNode*>(&probe);
                const Sensors::DisasterTelemetryDevice* viaLoRa =
                    static_cast<const Sensors::WirelessLoRaTransceiver*>(&probe);
                std::cout << "Diamond check: root via SolarPoweredNode == root via LoRa ? "
                          << (viaSolar == viaLoRa ? "YES (single shared root)" : "NO") << "\n";
                std::cout << "Device id=" << probe.deviceId() << " firmware=" << probe.firmware()
                          << " sizeof(AutonomousHybridProbe)=" << sizeof(probe) << " bytes\n";
            }

            std::map<int, std::map<std::string, double>> latest;
            std::map<int, double> prevRiver;
            Alerts::IncidentLogger logger("incidents.dat");
            Sensors::TelemetryAuditor auditor;

            const int kCycles = 5;
            for (int cycle = 1; cycle <= kCycles; ++cycle) {
                std::cout << "\n--- Sweep cycle " << cycle << " ---\n";
                if (cycle == 2) fleet.nodes[idx(2, 2)]->injectCorruption();   // radio bit-flip
                if (cycle == 3) fleet.nodes[idx(3, 0)]->injectFailure();      // hardware fault

                for (Sensors::BaseSensorNode* n : fleet.nodes) {              // dynamic dispatch via vtable
                    try {
                        double v = n->readTelemetry();
                        latest[n->stationId()][n->typeName()] = v;
                        bool warn = n->evaluateThreshold();
                        if (cycle == 1)
                            std::cout << "  st" << n->stationId() << " " << std::left << std::setw(22)
                                      << n->typeName() << std::right << std::setw(7) << v
                                      << (warn ? "  [ABOVE WARN]" : "") << "\n";
                    } catch (const SensorFailureException& e) {
                        std::cout << "  ! st" << n->stationId() << " " << e.what() << "\n";
                    } catch (const TelemetryChecksumMismatch& e) {
                        std::cout << "  ! st" << n->stationId() << " " << e.what() << " -> packet dropped\n";
                    } catch (const ThresholdOverflowException& e) {
                        std::cout << "  ! st" << n->stationId() << " " << n->typeName() << " " << e.what() << "\n";
                    } catch (const std::exception& e) {
                        std::cout << "  ! unexpected: " << e.what() << "\n";
                    }
                }

                // fuse sensor values into a station hazard using the overloaded functions
                for (auto& st : stations) {
                    const StationSpec& sp = *specById[st.id()];
                    auto& m = latest[st.id()];
                    double sat = m["SoilMoistureSensor"], vib = m["SeismicGeophone"];
                    double river = m["RiverSonarGauge"], tilt = m["AutonomousHybridProbe"];
                    double rise = std::max(0.0, river - prevRiver[st.id()]) * 10.0;
                    prevRiver[st.id()] = river;
                    double h = 0.8 * Sensors::computeHazardIndex(
                                         Sensors::RainfallSlope{sp.rain, std::max(tilt, sp.slope)},
                                         Sensors::SoilVibration{sat, vib})
                             + 0.2 * Sensors::computeHazardIndex(river, rise);
                    st.setHazard(h);
                    st.recordReading(h);
                }

                // STL: for_each + functor to sound sirens, sort by evacuation priority
                std::for_each(stations.begin(), stations.end(), Alerts::SirenActivator{dispatcher});
                std::vector<Sensors::SensorStation> ranked(stations);
                std::sort(ranked.begin(), ranked.end(), Alerts::EvacuationPriority());
                Alerts::printDispatchTable(std::cout, ranked, dispatcher);

                for (const auto& st : ranked)
                    if (st.level() >= AlertLevel::Warning)
                        logger.append(Alerts::makeRecord(st.id(), st.level(), st.hazard(),
                                      st.name() + " " + toString(st.level())));
            }

            // --- Unit III extras: dynamic_cast + containership ---
            banner("UNIT III: RTTI over the fleet and contained PowerSubsystem");
            for (auto* n : fleet.nodes)
                if (auto* h = dynamic_cast<Sensors::AutonomousHybridProbe*>(n))
                    std::cout << "  probe #" << h->deviceId() << " st" << h->stationId()
                              << " LoRa packets=" << h->packetsSent() << " @" << h->frequencyMHz()
                              << "MHz battery=" << h->powerStatus().percent() << "%\n";

            // --- Unit II operators ---
            banner("UNIT II: operator +, >, <<, >> and static members");
            Sensors::ZoneVector zone = stations[0] + stations[1] + stations[2];
            zone.rename("Garhwal-Himachal");
            std::cout << zone << "\n";
            std::cout << "stations[0] > stations[3] ? " << (stations[0] > stations[3] ? "yes" : "no") << "\n";
            { std::ofstream out("stations_snapshot.txt"); out << std::fixed << std::setprecision(2);
              for (const auto& s : stations) out << s << '\n'; }
            { std::ifstream in("stations_snapshot.txt"); Sensors::SensorStation restored;
              in >> restored; std::cout << "Restored from file via operator>>: " << restored << "\n"; }
            Sensors::translateCoordinate(stations[0].location(), 0.5, 0.5);  // return-by-ref accessor
            std::cout << "Active stations (static): " << Sensors::SensorStation::activeCount()
                      << " | Global alert (static): " << Sensors::SensorStation::globalAlert()
                      << " | Siren activations: " << Alerts::EarlyWarningSiren::totalActivations() << "\n";
            for (const auto& s : stations) auditor.inspect(s, std::cout);     // friend class

            // --- Unit IV: STL grouping + evacuation route ---
            banner("UNIT IV: STL, functors, evacuation routing");
            std::map<AlertLevel, std::vector<std::string>> byLevel;
            for (const auto& s : stations) byLevel[s.level()].push_back(s.name());
            for (const auto& kv : byLevel) {
                std::cout << "  " << std::left << std::setw(9) << kv.first << ": ";
                for (const auto& nm : kv.second) std::cout << nm << ' ';
                std::cout << "\n";
            }
            std::vector<Sensors::SensorStation> route(stations);
            std::sort(route.begin(), route.end(), Alerts::EvacuationPriority());
            long critical = std::count_if(route.begin(), route.end(),
                                          Alerts::CountAtOrAbove{AlertLevel::Warning});
            std::cout << "Evacuation order (" << critical << " zones at WARNING or above): ";
            for (const auto& s : route)
                if (s.level() >= AlertLevel::Warning) std::cout << s.name() << " -> ";
            std::cout << "END\n";

            // --- Unit IV: persistence + transactional rollback ---
            banner("UNIT IV: binary persistence with rollback");
            std::uint32_t before = logger.count();
            std::cout << "Committed incidents before crash test: " << before << "\n";
            try {
                logger.append(Alerts::makeRecord(1, AlertLevel::Evacuate, 99.0, "crash-test record"), true);
            } catch (const PersistenceException& e) {
                std::cout << "Caught: " << e.what() << "\n";
            }
            std::cout << "Committed incidents after rollback   : " << logger.count()
                      << (logger.count() == before ? "  (file intact)" : "  (ERROR)") << "\n";
            auto records = logger.readAll();
            std::cout << "Re-read " << records.size() << " records from incidents.dat (checksums verified)\n";
            std::cout << "  " << std::left << std::setw(5) << "stn" << std::setw(10) << "level"
                      << std::setw(8) << "hazard" << "message\n";
            for (std::size_t i = 0; i < records.size() && i < 6; ++i)
                std::cout << "  " << std::left << std::setw(5) << records[i].stationId << std::setw(10)
                          << toString(static_cast<AlertLevel>(records[i].level)) << std::setw(8)
                          << records[i].hazard << records[i].message << "\n";
        }   // stations, fleet (virtual dtors), logger all destroyed here

        banner("RESOURCE REPORT");
        std::cout << "Active stations after cleanup : " << Sensors::SensorStation::activeCount() << "\n";
        std::cout << "Live telemetry buffers        : " << Sensors::TelemetryBuffer::liveBuffers() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
