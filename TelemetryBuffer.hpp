// TelemetryBuffer.hpp - Unit II: dynamic memory, deep copy, destructor (ring buffer)
#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace DisasterCore {
namespace Sensors {

class TelemetryAuditor;  // friend (see SensorStation.hpp)

class TelemetryBuffer {
    double*     data_;
    std::size_t capacity_;
    std::size_t count_;
    std::size_t start_;                       // index of oldest sample
    static inline int liveBuffers_ = 0;       // leak-check counter

    friend class TelemetryAuditor;

public:
    // parameterized constructor
    explicit TelemetryBuffer(std::size_t capacity = 8)
        : data_(nullptr), capacity_(capacity ? capacity : 1), count_(0), start_(0) {
        data_ = new double[capacity_]();
        ++liveBuffers_;
    }

    // dynamic constructor: size decided at run time from external data
    TelemetryBuffer(const double* src, std::size_t n)
        : data_(nullptr), capacity_(n ? n : 1), count_(n), start_(0) {
        data_ = new double[capacity_]();
        std::copy(src, src + n, data_);
        ++liveBuffers_;
    }

    // deep copy constructor
    TelemetryBuffer(const TelemetryBuffer& o)
        : data_(new double[o.capacity_]), capacity_(o.capacity_), count_(o.count_), start_(o.start_) {
        std::copy(o.data_, o.data_ + o.capacity_, data_);
        ++liveBuffers_;
    }

    // copy assignment (copy-and-swap => strong exception safety)
    TelemetryBuffer& operator=(const TelemetryBuffer& o) {
        if (this != &o) {
            TelemetryBuffer tmp(o);
            std::swap(data_, tmp.data_);
            std::swap(capacity_, tmp.capacity_);
            std::swap(count_, tmp.count_);
            std::swap(start_, tmp.start_);
        }
        return *this;
    }

    // explicit destructor
    ~TelemetryBuffer() { delete[] data_; --liveBuffers_; }

    void push(double v) {
        if (count_ < capacity_) { data_[(start_ + count_) % capacity_] = v; ++count_; }
        else { data_[start_] = v; start_ = (start_ + 1) % capacity_; }
    }

    // return by reference (i-th oldest sample)
    double& at(std::size_t i) {
        if (i >= count_) throw std::out_of_range("TelemetryBuffer::at");
        return data_[(start_ + i) % capacity_];
    }
    const double& at(std::size_t i) const {
        if (i >= count_) throw std::out_of_range("TelemetryBuffer::at");
        return data_[(start_ + i) % capacity_];
    }

    std::size_t size() const { return count_; }
    std::size_t capacity() const { return capacity_; }
    double mean() const {
        if (!count_) return 0.0;
        double s = 0; for (std::size_t i = 0; i < count_; ++i) s += at(i);
        return s / static_cast<double>(count_);
    }
    double peak() const {
        double p = 0; for (std::size_t i = 0; i < count_; ++i) p = std::max(p, at(i));
        return p;
    }
    static int liveBuffers() { return liveBuffers_; }
};

}  // namespace Sensors
}  // namespace DisasterCore
