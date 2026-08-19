#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace panpath {

struct MemoryTelemetry { std::uint64_t limit_bytes; std::uint64_t peak_tracked_bytes; std::uint64_t oversized_contigs; };

class MemoryPermit {
public:
    MemoryPermit() = default;
    ~MemoryPermit();
    MemoryPermit(MemoryPermit&&) noexcept;
    MemoryPermit& operator=(MemoryPermit&&) noexcept;
    MemoryPermit(const MemoryPermit&) = delete;
private:
    struct Inner;
    friend class MemoryBudget;
    MemoryPermit(std::shared_ptr<Inner> inner, std::uint64_t bytes);
    std::shared_ptr<Inner> inner_;
    std::uint64_t bytes_ = 0;
};

class MemoryBudget {
public:
    explicit MemoryBudget(std::uint64_t limit_bytes);
    MemoryPermit acquire(std::uint64_t bytes);
    std::optional<MemoryPermit> acquire_within_limit(std::uint64_t bytes);
    MemoryTelemetry telemetry() const;
private:
    std::shared_ptr<MemoryPermit::Inner> inner_;
};

}
