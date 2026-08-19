#include "panpath/spool.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace panpath {

Spool::Spool(const std::filesystem::path& directory) {
    auto pattern = (directory / "panpath-audit-cpp-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    fd_ = mkstemp(name.data());
    if (fd_ < 0) throw std::runtime_error("TEMP_STORE path=" + directory.string() + " error=" + std::strerror(errno));
    if (unlink(name.data()) != 0) {
        const auto message = std::string(std::strerror(errno));
        close(fd_); fd_ = -1;
        throw std::runtime_error("TEMP_STORE path=" + directory.string() + " error=" + message);
    }
}

Spool::~Spool() { if (fd_ >= 0) close(fd_); }

RecordRef Spool::append(std::span<const char> bytes) {
    const RecordRef record{end_, bytes.size()};
    std::size_t written = 0;
    while (written < bytes.size()) {
        const auto count = pwrite(fd_, bytes.data() + written, bytes.size() - written, static_cast<off_t>(end_ + written));
        if (count < 0) throw std::runtime_error(std::strerror(errno));
        written += static_cast<std::size_t>(count);
    }
    end_ += bytes.size();
    return record;
}

std::string Spool::read(RecordRef record) const {
    std::string result(record.length, '\0');
    std::size_t received = 0;
    while (received < result.size()) {
        const auto count = pread(fd_, result.data() + received, result.size() - received,
                                 static_cast<off_t>(record.offset + received));
        if (count <= 0) throw std::runtime_error(count == 0 ? "unexpected end of spool" : std::strerror(errno));
        received += static_cast<std::size_t>(count);
    }
    return result;
}
}
