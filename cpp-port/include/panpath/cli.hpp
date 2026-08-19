#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace panpath {

enum class Format { human, json, tsv };
enum class SourceMode { exact, pansn, prefix };

struct SourceSpec {
    std::filesystem::path path;
    SourceMode mode = SourceMode::exact;
    std::string sample;
    std::string haplotype;
    std::string prefix;

    std::string identifier(const std::string& raw) const;
};

struct Config {
    Format format = Format::human;
    bool comprehensive = false;
    std::uint64_t alignment_max_cells = 10'000'000;
    std::size_t threads = 1;
    std::uint64_t memory_mib = 1024;
    std::filesystem::path temp_dir;
    std::vector<SourceSpec> sources;
    std::filesystem::path gfa_path;
};

const char* help_text();
Config parse_arguments(const std::vector<std::string>& arguments);

}
