#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

namespace panpath {

struct RecordRef { std::uint64_t offset; std::uint64_t length; };

class Spool {
public:
    explicit Spool(const std::filesystem::path& directory);
    ~Spool();
    Spool(const Spool&) = delete;
    Spool& operator=(const Spool&) = delete;

    RecordRef append(std::span<const char> bytes);
    std::string read(RecordRef record) const;

private:
    int fd_ = -1;
    std::uint64_t end_ = 0;
};

}
