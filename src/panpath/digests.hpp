#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace panpath {

struct SequenceDigest { std::string sha256, blake3; };

class SequenceHasher {
public:
    SequenceHasher();
    ~SequenceHasher();
    SequenceHasher(SequenceHasher&&) noexcept;
    SequenceHasher& operator=(SequenceHasher&&) noexcept;
    SequenceHasher(const SequenceHasher&) = delete;
    void update(std::span<const std::uint8_t> sequence);
    SequenceDigest finish();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

SequenceDigest sequence_digest(std::span<const std::uint8_t> sequence);

}
