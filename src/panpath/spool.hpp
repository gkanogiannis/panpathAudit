#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>

namespace panpath {

struct RecordRef { std::uint64_t offset, length; };

class Spool {
public:
    explicit Spool(const std::filesystem::path& directory);
    ~Spool();
    Spool(const Spool&) = delete;
    Spool& operator=(const Spool&) = delete;
    RecordRef append(std::span<const char> bytes);
    std::string read(RecordRef record) const;
    const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::FILE* file_ = nullptr;
    std::filesystem::path path_;
    mutable std::mutex mutex_;
    std::uint64_t end_ = 0;
};

}
