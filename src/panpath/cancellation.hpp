#pragma once

#include <atomic>
#include <functional>
#include <stdexcept>

namespace panpath {

using ProgressCallback = std::function<void()>;

class Cancelled final : public std::runtime_error {
public:
    Cancelled() : std::runtime_error("audit cancelled") {}
};

class Cancellation {
public:
    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }
    bool cancelled() const noexcept { return cancelled_.load(std::memory_order_acquire); }
    void check() const { if (cancelled()) throw Cancelled(); }
private:
    std::atomic<bool> cancelled_{false};
};

}
