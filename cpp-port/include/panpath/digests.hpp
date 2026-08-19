#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <memory>

namespace panpath {

struct SequenceDigest {
    std::string sha256;
    std::string blake3;
};

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
