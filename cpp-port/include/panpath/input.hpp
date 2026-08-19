#pragma once

#include "panpath/cli.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace panpath {

enum class Compression { plain, gzip };

class LineReader {
public:
    explicit LineReader(const std::filesystem::path& path);
    ~LineReader();
    LineReader(LineReader&&) noexcept;
    LineReader& operator=(LineReader&&) noexcept;
    LineReader(const LineReader&) = delete;
    bool read(std::string& line);
    Compression compression() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

using FastaVisitor = std::function<void(const std::string&, const std::string&)>;
void stream_fasta(const std::filesystem::path& path, const FastaVisitor& visitor);

struct SourceIndex {
    std::map<std::string, std::size_t> lengths;
    std::vector<std::string> compressions;
    std::vector<std::string> errors;
};

SourceIndex index_sources(const std::vector<SourceSpec>& sources);

}
