#include "panpath/memory.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace panpath {

struct MemoryPermit::Inner {
    explicit Inner(std::uint64_t value) : limit(value) {}
    std::uint64_t limit, used = 0, peak = 0, oversized = 0;
    mutable std::mutex mutex;
    std::condition_variable available;
};

MemoryPermit::MemoryPermit(std::shared_ptr<Inner> inner, std::uint64_t bytes)
    : inner_(std::move(inner)), bytes_(bytes) {}

MemoryPermit::~MemoryPermit() {
    if (!inner_) return;
    {
        std::lock_guard lock(inner_->mutex);
        inner_->used -= bytes_;
    }
    inner_->available.notify_all();
}

MemoryPermit::MemoryPermit(MemoryPermit&& other) noexcept
    : inner_(std::move(other.inner_)), bytes_(other.bytes_) {
    other.bytes_ = 0;
}

MemoryPermit& MemoryPermit::operator=(MemoryPermit&& other) noexcept {
    if (this != &other) {
        MemoryPermit replacement(std::move(other));
        inner_.swap(replacement.inner_);
        std::swap(bytes_, replacement.bytes_);
    }
    return *this;
}

MemoryBudget::MemoryBudget(std::uint64_t limit_bytes)
    : inner_(std::make_shared<MemoryPermit::Inner>(limit_bytes)) {}

MemoryPermit MemoryBudget::acquire(std::uint64_t bytes, const Cancellation& cancellation,
                                   const ProgressCallback& progress) {
    std::unique_lock lock(inner_->mutex);
    const auto available = [&] {
        return inner_->used == 0 ||
            (bytes <= inner_->limit && inner_->used <= inner_->limit - bytes);
    };
    while (!available()) {
        cancellation.check();
        lock.unlock();
        if (progress) progress();
        lock.lock();
        inner_->available.wait_for(lock, std::chrono::milliseconds(25));
    }
    cancellation.check();
    inner_->used += bytes;
    inner_->peak = std::max(inner_->peak, inner_->used);
    inner_->oversized += bytes > inner_->limit;
    return {inner_, bytes};
}

std::optional<MemoryPermit> MemoryBudget::acquire_within_limit(
    std::uint64_t bytes, const Cancellation& cancellation) {
    if (bytes > inner_->limit) return std::nullopt;
    return acquire(bytes, cancellation);
}

MemoryTelemetry MemoryBudget::telemetry() const {
    std::lock_guard lock(inner_->mutex);
    return {inner_->limit, inner_->peak, inner_->oversized};
}

}
