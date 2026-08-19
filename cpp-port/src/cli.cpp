#include "panpath/cli.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <thread>

namespace panpath {
namespace {
std::uint64_t positive(const std::string& value, const char* message) {
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || result == 0)
        throw std::runtime_error(message);
    return result;
}

const std::string& take(const std::vector<std::string>& args, std::size_t& index, const char* message) {
    if (++index == args.size()) throw std::runtime_error(message);
    return args[index];
}
}

std::string SourceSpec::identifier(const std::string& raw) const {
    if (mode == SourceMode::pansn) return sample + "#" + haplotype + "#" + raw;
    if (mode == SourceMode::prefix) return prefix + raw;
    return raw;
}

const char* help_text() {
    return R"(panpath-audit - audit GFA traversals against source FASTA sequences

Usage:
  panpath-audit [OPTIONS] [FASTA ...] GFA

Inputs:
      --fasta FILE                    Add a FASTA with exact identifiers
      --fasta-pansn SAMPLE HAP FILE   Add FASTA using SAMPLE#HAP#identifier
      --fasta-prefix PREFIX FILE      Add FASTA using PREFIXidentifier

Output:
      --format human|json|tsv         Output format [default: human]
      --stats comprehensive           Add exact base statistics and digests

Resources:
      --threads N                     Worker threads [default: CPUs, max 8]
      --memory-mib N                  Tracked sequence memory [default: 1024]
      --alignment-max-cells N         Alignment limit [default: 10000000]
      --temp-dir DIR                  Scratch directory [default: OS temp]

General:
  -h, --help                          Print help
  -V, --version                       Print version
      --                               Treat remaining arguments as paths

Positional FASTA inputs use exact identifiers. Inputs may be plain or gzip.)";
}

Config parse_arguments(const std::vector<std::string>& args) {
    Config config;
    config.threads = std::min<std::size_t>(std::max(1u, std::thread::hardware_concurrency()), 8);
    config.temp_dir = std::filesystem::temp_directory_path();
    bool format_seen = false, threads_seen = false, memory_seen = false, temp_seen = false;
    bool stats_seen = false, cells_seen = false, positional_only = false;
    std::vector<std::string> positional;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (positional_only) positional.push_back(arg);
        else if (arg == "--") positional_only = true;
        else if (arg == "--format") {
            if (format_seen) throw std::runtime_error("format specified more than once");
            const auto& value = take(args, i, "missing format");
            if (value == "human") config.format = Format::human;
            else if (value == "json") config.format = Format::json;
            else if (value == "tsv") config.format = Format::tsv;
            else throw std::runtime_error("format must be human, json, or tsv");
            format_seen = true;
        } else if (arg == "--threads") {
            if (threads_seen) throw std::runtime_error("threads specified more than once");
            config.threads = positive(take(args, i, "missing thread count"), "threads must be a positive integer");
            threads_seen = true;
        } else if (arg == "--memory-mib") {
            if (memory_seen) throw std::runtime_error("memory specified more than once");
            config.memory_mib = positive(take(args, i, "missing memory limit"), "memory must be a positive integer in MiB");
            memory_seen = true;
        } else if (arg == "--temp-dir") {
            if (temp_seen) throw std::runtime_error("temporary directory specified more than once");
            config.temp_dir = take(args, i, "missing temporary directory"); temp_seen = true;
        } else if (arg == "--stats") {
            if (stats_seen) throw std::runtime_error("stats specified more than once");
            if (take(args, i, "missing stats mode") != "comprehensive") throw std::runtime_error("stats must be comprehensive");
            config.comprehensive = true; stats_seen = true;
        } else if (arg == "--alignment-max-cells") {
            if (cells_seen) throw std::runtime_error("alignment max cells specified more than once");
            config.alignment_max_cells = positive(take(args, i, "missing alignment max cells"), "alignment max cells must be a positive integer");
            cells_seen = true;
        } else if (arg == "--fasta") {
            SourceSpec source; source.path = take(args, i, "missing --fasta file"); config.sources.push_back(std::move(source));
        } else if (arg == "--fasta-pansn") {
            SourceSpec source; source.mode = SourceMode::pansn;
            source.sample = take(args, i, "missing --fasta-pansn sample");
            source.haplotype = take(args, i, "missing --fasta-pansn haplotype");
            source.path = take(args, i, "missing --fasta-pansn file"); config.sources.push_back(source);
        } else if (arg == "--fasta-prefix") {
            SourceSpec source; source.mode = SourceMode::prefix;
            source.prefix = take(args, i, "missing --fasta-prefix prefix");
            source.path = take(args, i, "missing --fasta-prefix file"); config.sources.push_back(source);
        } else if (!arg.empty() && arg[0] == '-') throw std::runtime_error("unknown option " + arg);
        else positional.push_back(arg);
    }
    if (positional.empty()) throw std::runtime_error("missing GFA");
    config.gfa_path = positional.back(); positional.pop_back();
    for (const auto& path : positional) { SourceSpec source; source.path = path; config.sources.push_back(std::move(source)); }
    if (config.sources.empty()) throw std::runtime_error("missing source FASTA");
    if (cells_seen && !config.comprehensive) throw std::runtime_error("alignment max cells requires --stats comprehensive");
    return config;
}

}
