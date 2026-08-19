#include "panpath/digests.hpp"

#include "blake3.h"
#include "sha-256.h"

#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

namespace panpath {
struct SequenceHasher::Impl {
    std::array<std::uint8_t, SIZE_OF_SHA_256_HASH> sha{};
    Sha_256 sha_state;
    blake3_hasher blake_state;
    Impl() { sha_256_init(&sha_state, sha.data()); blake3_hasher_init(&blake_state); }
};
namespace {
std::string hex(const std::uint8_t* bytes, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) output << std::setw(2) << unsigned(bytes[i]);
    return output.str();
}
}

SequenceHasher::SequenceHasher() : impl_(std::make_unique<Impl>()) {}
SequenceHasher::~SequenceHasher() = default;
SequenceHasher::SequenceHasher(SequenceHasher&&) noexcept = default;
SequenceHasher& SequenceHasher::operator=(SequenceHasher&&) noexcept = default;
void SequenceHasher::update(std::span<const std::uint8_t> sequence) {
    std::array<std::uint8_t, 4096> buffer{};
    for (std::size_t offset = 0; offset < sequence.size();) {
        const auto count = std::min(buffer.size(), sequence.size() - offset);
        for (std::size_t i = 0; i < count; ++i) buffer[i] = static_cast<std::uint8_t>(std::toupper(sequence[offset + i]));
        sha_256_write(&impl_->sha_state, buffer.data(), count);
        blake3_hasher_update(&impl_->blake_state, buffer.data(), count);
        offset += count;
    }
}
SequenceDigest SequenceHasher::finish() {
    sha_256_close(&impl_->sha_state);
    std::array<std::uint8_t, BLAKE3_OUT_LEN> blake{};
    blake3_hasher_finalize(&impl_->blake_state, blake.data(), blake.size());
    return {hex(impl_->sha.data(), impl_->sha.size()), hex(blake.data(), blake.size())};
}

SequenceDigest sequence_digest(std::span<const std::uint8_t> sequence) {
    SequenceHasher hasher;
    hasher.update(sequence);
    return hasher.finish();
}

}
