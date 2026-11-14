// IncidentLogger.hpp - Unit IV: binary fstream persistence with transactional rollback
//
// File layout:  [FileHeader (16 B)] [IncidentRecord 0] [IncidentRecord 1] ...
// A record is only "committed" when the header's count is rewritten (seekp(0)).
// If anything fails mid-write, the partial record is erased and the header is
// reloaded (seekg(0)), so the file always stays consistent.
#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include "Common.hpp"
#include "Exceptions.hpp"

namespace DisasterCore {
namespace Alerts {

struct IncidentRecord {
    double        hazard;
    std::int64_t  timestamp;
    std::int32_t  stationId;
    std::int32_t  level;
    char          message[44];
    std::uint32_t checksum;
};

struct FileHeader {
    char          magic[8];
    std::uint32_t count;
    std::uint32_t reserved;
};

inline std::uint32_t recordChecksum(const IncidentRecord& r) {
    std::uint32_t h = checksum32(&r.hazard, sizeof r.hazard);
    h = checksum32(&r.timestamp, sizeof r.timestamp, h);
    h = checksum32(&r.stationId, sizeof r.stationId, h);
    h = checksum32(&r.level, sizeof r.level, h);
    h = checksum32(r.message, sizeof r.message, h);
    return h;
}

inline IncidentRecord makeRecord(int stationId, AlertLevel level, double hazard, const std::string& msg) {
    IncidentRecord r;
    std::memset(&r, 0, sizeof r);
    r.hazard = hazard;
    r.timestamp = static_cast<std::int64_t>(std::time(nullptr));
    r.stationId = stationId;
    r.level = static_cast<std::int32_t>(level);
    std::strncpy(r.message, msg.c_str(), sizeof r.message - 1);
    return r;
}

class IncidentLogger {
    std::string   path_;
    std::fstream  file_;
    FileHeader    header_;

    std::streamoff offsetOf(std::uint32_t idx) const {
        return static_cast<std::streamoff>(sizeof(FileHeader) + idx * sizeof(IncidentRecord));
    }
    void writeHeader() {
        file_.clear();
        file_.seekp(0);
        file_.write(reinterpret_cast<const char*>(&header_), sizeof header_);
        file_.flush();
        if (!file_) throw PersistenceException("header write failed");
    }
    void loadHeader() {
        file_.clear();
        file_.seekg(0);
        file_.read(reinterpret_cast<char*>(&header_), sizeof header_);
        if (!file_ || std::memcmp(header_.magic, "ALFEWG1", 8) != 0)
            throw PersistenceException("corrupt or missing file header");
    }
    void rollback(std::streamoff pos) {
        file_.clear();
        file_.seekp(pos);
        std::vector<char> zeros(sizeof(IncidentRecord), 0);
        file_.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
        file_.flush();
        try { loadHeader(); } catch (...) { /* keep in-memory header */ }
    }

public:
    explicit IncidentLogger(const std::string& path) : path_(path) {
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            { std::ofstream create(path_, std::ios::binary); }
            file_.clear();
            file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
            if (!file_.is_open()) throw PersistenceException("cannot open " + path_);
            std::memset(&header_, 0, sizeof header_);
            std::memcpy(header_.magic, "ALFEWG1", 8);
            writeHeader();
        } else {
            loadHeader();
        }
    }
    IncidentLogger(const IncidentLogger&) = delete;
    IncidentLogger& operator=(const IncidentLogger&) = delete;
    ~IncidentLogger() { if (file_.is_open()) file_.close(); }

    std::uint32_t count() const { return header_.count; }

    void append(const IncidentRecord& rec, bool simulateCrash = false) {
        const std::streamoff pos = offsetOf(header_.count);
        IncidentRecord out = rec;
        out.checksum = recordChecksum(out);
        try {
            file_.clear();
            file_.seekp(pos);
            const char* raw = reinterpret_cast<const char*>(&out);
            const std::size_t half = sizeof out / 2;
            file_.write(raw, static_cast<std::streamsize>(half));
            if (simulateCrash) throw PersistenceException("simulated power loss mid-write");
            file_.write(raw + half, static_cast<std::streamsize>(sizeof out - half));
            file_.flush();
            if (!file_) throw PersistenceException("record write failed");
            ++header_.count;
            writeHeader();                       // commit point
        } catch (...) {
            rollback(pos);                       // undo partial write, restore header
            throw;
        }
    }

    std::vector<IncidentRecord> readAll() {
        loadHeader();
        std::vector<IncidentRecord> out;
        out.reserve(header_.count);
        for (std::uint32_t i = 0; i < header_.count; ++i) {
            IncidentRecord r;
            file_.clear();
            file_.seekg(offsetOf(i));
            file_.read(reinterpret_cast<char*>(&r), sizeof r);
            if (!file_) throw PersistenceException("truncated record");
            std::uint32_t calc = recordChecksum(r);
            if (calc != r.checksum) throw TelemetryChecksumMismatch(r.checksum, calc);
            out.push_back(r);
        }
        return out;
    }
};

}  // namespace Alerts
}  // namespace DisasterCore
