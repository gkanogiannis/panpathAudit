#include "panpath/spool.hpp"

#include <cerrno>
#include <cstring>
#include <random>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace panpath {
namespace {

std::string random_name() {
    static constexpr char digits[] = "0123456789abcdef";
    std::random_device random;
    std::string value = "panpathAudit-";
    for (int i = 0; i < 32; ++i) value.push_back(digits[random() & 15U]);
    value += ".tmp";
    return value;
}

std::FILE* create_exclusive(const std::filesystem::path& path) {
    int descriptor = -1;
#ifdef _WIN32
    const auto error = _wsopen_s(&descriptor, path.c_str(),
        _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY, _SH_DENYNO,
        _S_IREAD | _S_IWRITE);
    if (error != 0) { errno = error; return nullptr; }
    auto* file = _fdopen(descriptor, "w+b");
    if (!file) _close(descriptor);
#else
    descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (descriptor < 0) return nullptr;
    auto* file = fdopen(descriptor, "w+b");
    if (!file) ::close(descriptor);
#endif
    return file;
}

int seek64(std::FILE* file, std::uint64_t offset) {
#ifdef _WIN32
    return _fseeki64(file, static_cast<__int64>(offset), SEEK_SET);
#else
    return fseeko(file, static_cast<off_t>(offset), SEEK_SET);
#endif
}

}

Spool::Spool(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
        throw std::runtime_error("TEMP_STORE directory is unavailable: " + directory.string());
    for (int attempt = 0; attempt < 128 && !file_; ++attempt) {
        path_ = directory / random_name();
        file_ = create_exclusive(path_);
        if (!file_ && errno != EEXIST)
            throw std::runtime_error("TEMP_STORE " + path_.string() + ": " + std::strerror(errno));
    }
    if (!file_) throw std::runtime_error("TEMP_STORE could not create a unique file");
}

Spool::~Spool() {
    if (file_) std::fclose(file_);
    std::error_code error;
    if (!path_.empty()) std::filesystem::remove(path_, error);
}

RecordRef Spool::append(std::span<const char> bytes) {
    std::lock_guard lock(mutex_);
    const RecordRef record{end_, bytes.size()};
    if (seek64(file_, end_) != 0) throw std::runtime_error("TEMP_STORE seek failed");
    if (!bytes.empty() && std::fwrite(bytes.data(), 1, bytes.size(), file_) != bytes.size())
        throw std::runtime_error("TEMP_STORE write failed");
    if (std::fflush(file_) != 0) throw std::runtime_error("TEMP_STORE flush failed");
    end_ += bytes.size();
    return record;
}

std::string Spool::read(RecordRef record) const {
    std::lock_guard lock(mutex_);
    if (record.offset > end_ || record.length > end_ - record.offset)
        throw std::runtime_error("TEMP_STORE record is out of bounds");
    std::string result(record.length, '\0');
    if (seek64(file_, record.offset) != 0) throw std::runtime_error("TEMP_STORE seek failed");
    if (!result.empty() && std::fread(result.data(), 1, result.size(), file_) != result.size())
        throw std::runtime_error("TEMP_STORE read failed");
    return result;
}

}
