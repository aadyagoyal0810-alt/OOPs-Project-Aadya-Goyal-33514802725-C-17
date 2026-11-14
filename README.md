# ALF-EWG — Autonomous Landslide & Flash-Flood Early Warning Grid
**Course:** Object-Oriented Programming Using C++ (CIC-211) · **Project Option 1** · MAIT, Dept. of CSE
**Team:** _<Name 1, Enrollment No.>_ , _<Name 2, Enrollment No.>_  **Submit before:** 27 Nov 2026, 11:59 PM

## 1. Overview
A backend that ingests telemetry from solar-powered IoT nodes (soil moisture, seismic geophones,
acoustic tiltmeters, river sonar), fuses readings into a per-station **hazard index (0-100)**, ranks stations
for evacuation, sounds regional sirens, and persists incidents to a checksummed binary log with
transactional rollback.

## 2. Build & Run
```bash
make          # g++ -std=c++17 -Wall -Wextra -pedantic
./alf_ewg     # runs the full 5-cycle simulation (see sample_output.txt)
make clean
```
Requires any C++17 compiler (uses `static inline` members). Output files: `incidents.dat`, `stations_snapshot.txt`.

## 3. File Structure
| File | Contents |
|---|---|
| `Common.hpp` | `AlertLevel`, inline `levelFromHazard`, FNV-1a `checksum32` |
| `Exceptions.hpp` | `SensorFailureException`, `TelemetryChecksumMismatch`, `ThresholdOverflowException`, `PersistenceException` |
| `Hazard.hpp` | `DisasterCore::Sensors` — inline risk maths, overloaded `computeHazardIndex`, by-reference helpers |
| `TelemetryBuffer.hpp` | Dynamic ring buffer (parameterized/dynamic/deep-copy ctors, destructor) |
| `SensorStation.hpp` | `SensorStation`, `TelemetryAuditor` (friend), `ZoneVector`, operators |
| `Nodes.hpp` | `BaseSensorNode`, diamond hierarchy, `PowerSubsystem`, concrete sensors |
| `Alerts.hpp` | `DisasterCore::Alerts` — `EarlyWarningSiren`, `AlertDispatcher`, functors, manipulators |
| `IncidentLogger.hpp` | Binary `std::fstream` persistence with rollback |
| `main.cpp` | Driver / demonstration of every requirement |

## 4. Requirement → Implementation Map
### Unit I
| Requirement | Where |
|---|---|
| Namespaces `DisasterCore::Sensors`, `::Alerts` | `Hazard.hpp`, `Nodes.hpp`, `Alerts.hpp` |
| Inline functions for risk indexing | `clamp01`, `weighted`, all `computeHazardIndex` (`Hazard.hpp`) |
| Overloaded `computeHazardIndex()` | 4 overloads: (rain+slope), (saturation+vibration), (river level+rise), combined |
| Call / return by reference | `translateCoordinate(GeoCoord&,…)`, `accumulate(TelemetryState&,…)` returns `TelemetryState&`, `TelemetryBuffer::at()`, `SensorStation::location()` |
| Friend class | `TelemetryAuditor` reads raw private buffer of `SensorStation`/`TelemetryBuffer` |

### Unit II
| Requirement | Where |
|---|---|
| Data abstraction + const correctness | `SensorStation`, `TelemetryBuffer`, `EarlyWarningSiren` — private data, const getters/overloads |
| Static members/functions | `SensorStation::activeCount()`, `globalAlert()`; `EarlyWarningSiren::totalActivations()`; `TelemetryBuffer::liveBuffers()` |
| Parameterized, dynamic, deep-copy ctors, destructor | `TelemetryBuffer` (`new[]/delete[]`, copy-and-swap assignment) |
| `operator+` | `Station+Station`, `Zone+Station`, `Zone+Zone` → `ZoneVector` |
| `operator>` | `SensorStation::operator>` (hazard comparison) |
| `<<` and `>>` | `SensorStation` (console + file streams), `ZoneVector`, `AlertLevel` |

### Unit III
| Requirement | Where |
|---|---|
| Abstract `BaseSensorNode` | pure virtual `readTelemetry()`, `evaluateThreshold()` (+ `typeName()`) |
| Diamond via virtual bases | `DisasterTelemetryDevice` ← `SolarPoweredNode`, `WirelessLoRaTransceiver` (virtual) ← `AutonomousHybridProbe` |
| Containership | `PowerSubsystem power_` inside `BaseSensorNode` |
| `std::vector<BaseSensorNode*>` dynamic dispatch | sweep loop in `main.cpp`; cleanup through virtual destructors (`FleetGuard`) |

### Unit IV
| Requirement | Where |
|---|---|
| Custom exceptions with `what()` | `Exceptions.hpp`; thrown in `BaseSensorNode::acquire/checkThreshold`, caught in sweep |
| Binary fstream + rollback with `seekp/seekg` | `IncidentLogger::append/rollback/readAll` (header-count commit point) |
| STL containers/algorithms | `vector`, `map`; `sort`, `for_each`, `count_if` |
| Functors | `EvacuationPriority`, `SirenActivator`, `CountAtOrAbove` |
| Custom manipulators | `tableRule`, `alertHeader`, parameterized `field{w}` |

## 5. Class Diagram (simplified)
```
DisasterTelemetryDevice  (root, virtual base)
   ^ virtual         ^ virtual              ^ virtual
BaseSensorNode  SolarPoweredNode   WirelessLoRaTransceiver
 (abstract,         \                   /
  has PowerSubsystem)  \               /
   ^                  AutonomousHybridProbe  (also derives BaseSensorNode)
   |-- SoilMoistureSensor / SeismicGeophone / RiverSonarGauge

SensorStation --owns--> TelemetryBuffer      ZoneVector --holds--> SensorStation*
AlertDispatcher --maps--> EarlyWarningSiren   IncidentLogger --writes--> IncidentRecord
TelemetryAuditor  (friend of SensorStation & TelemetryBuffer)
```

## 6. Design Notes
* **Hazard fusion:** `0.8 × combined(rain/slope, soil/vibration) + 0.2 × river(level, rise rate)`.
  Levels: <20 NORMAL, <40 WATCH, <65 WARNING, ≥65 EVACUATE.
* **Diamond:** the most-derived class (`AutonomousHybridProbe`) initializes the single virtual base.
* **Transaction:** record bytes are written first; only after success is the header `count` rewritten
  (`seekp(0)`). A failure erases the partial record and reloads the header (`seekg(0)`).
* **Fault injection:** cycle 2 → checksum mismatch, cycle 3 → sensor failure, cycle 5 → threshold overflow, plus a simulated power-loss during logging.

## 7. Likely Viva Questions
1. Why `virtual` inheritance? What happens to object size/layout (vptr/vbptr)?
2. Who initializes a virtual base, and why do all concrete ctors name it?
3. Why is `operator<<` a friend and not a member?
4. Rule of three: why copy-and-swap in `TelemetryBuffer::operator=`?
5. Difference between aggregation/composition (`PowerSubsystem`) and inheritance.
6. Why must the root destructor be `virtual` when deleting via `BaseSensorNode*`?
7. How does the rollback guarantee file consistency? What if the crash happens after the header write?
8. Return by reference — what danger exists (dangling reference) and how does the code avoid it?

## 8. Submission Checklist
- [ ] Fill team names/enrollment numbers above
- [ ] `git init && git add . && git commit -m "ALF-EWG final" && git push` to a GitHub repository
- [ ] Zip the folder (without the `alf_ewg` binary) and upload via the Google Form: https://forms.gle/cgWY4vZ3Siep8mmF9
- [ ] Deadline: 27 Nov 2026, 11:59 PM
