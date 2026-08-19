#include "panpath/graph.hpp"
#include "panpath/input.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace panpath {
namespace {
std::vector<std::string_view> fields(const std::string& line) {
    std::vector<std::string_view> result;
    const std::string_view input(line);
    std::size_t start = 0;
    while (true) {
        const auto end = input.find('\t', start);
        result.push_back(input.substr(start, end - start));
        if (end == std::string::npos) return result;
        start = end + 1;
    }
}

std::optional<std::size_t> number(std::string_view value) {
    std::size_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) return std::nullopt;
    return result;
}

std::optional<std::vector<Step>> path_steps(std::string_view value) {
    std::vector<Step> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(',', start);
        auto token = value.substr(start, end - start);
        if (token.size() < 2 || (token.back() != '+' && token.back() != '-')) return std::nullopt;
        const auto orientation = token.back() == '+' ? Orientation::forward : Orientation::reverse;
        token.remove_suffix(1);
        if (token.empty()) return std::nullopt;
        result.push_back({std::string(token), orientation});
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

std::optional<std::vector<Step>> parsed_walk(std::string_view value) {
    try {
        std::vector<Step> result;
        for (const auto& [name, orientation] : walk_steps(value)) result.push_back({std::string(name), orientation});
        return result;
    } catch (...) { return std::nullopt; }
}

void references(const std::string& path, const std::vector<Step>& steps,
                std::map<std::string, std::set<std::string>>& pending,
                const SegmentIndex& declared) {
    for (const auto& step : steps) if (!declared.contains(step.segment)) pending[step.segment].insert(path);
}
}

std::optional<std::size_t> SegmentIndex::numeric_name(const std::string& name) {
    auto parsed = number(name); if (!parsed || *parsed == 0) return std::nullopt; return parsed;
}

std::array<std::uint8_t, 12> SegmentIndex::pack(RecordRef record) {
    std::array<std::uint8_t, 12> packed{};
    for (std::size_t index = 0; index < 8; ++index) packed[index] = static_cast<std::uint8_t>(record.offset >> (index * 8));
    if (record.length > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("temporary record too large");
    for (std::size_t index = 0; index < 4; ++index) packed[index + 8] = static_cast<std::uint8_t>(record.length >> (index * 8));
    return packed;
}

RecordRef SegmentIndex::unpack(const std::array<std::uint8_t, 12>& packed) {
    RecordRef record{};
    for (std::size_t index = 0; index < 8; ++index) record.offset |= static_cast<std::uint64_t>(packed[index]) << (index * 8);
    for (std::size_t index = 0; index < 4; ++index) record.length |= static_cast<std::uint64_t>(packed[index + 8]) << (index * 8);
    return record;
}

bool SegmentIndex::missing(const std::array<std::uint8_t, 12>& record) {
    return std::all_of(record.begin(), record.end(), [](std::uint8_t value) { return value == 0; });
}

bool SegmentIndex::contains(const std::string& name) const {
    if (const auto numeric = numeric_name(name)) return *numeric < numeric_.size() && !missing(numeric_[*numeric]);
    return named_.contains(name);
}

bool SegmentIndex::insert(const std::string& name, RecordRef record) {
    if (const auto numeric = numeric_name(name)) {
        if (*numeric >= numeric_.size()) numeric_.resize(*numeric + 1);
        if (!missing(numeric_[*numeric])) return false;
        numeric_[*numeric] = pack(record); return true;
    }
    return named_.emplace(name, record).second;
}

RecordRef SegmentIndex::at(const std::string& name) const {
    if (const auto numeric = numeric_name(name)) {
        if (*numeric >= numeric_.size() || missing(numeric_[*numeric])) throw std::out_of_range("unknown segment");
        return unpack(numeric_[*numeric]);
    }
    return named_.at(name);
}

GraphReadResult read_graph(const std::filesystem::path& path,
                           const std::map<std::string, std::size_t>& source_lengths,
                           const std::filesystem::path& temp_dir) {
    GraphReadResult result;
    result.graph.segment_spool = std::make_shared<Spool>(temp_dir);
    result.graph.traversal_spool = std::make_shared<Spool>(temp_dir);
    LineReader reader(path);
    result.graph.compression = reader.compression() == Compression::gzip ? "gzip" : "plain";
    std::set<std::string> p_names, walk_names, unspecified;
    std::map<std::string, std::set<std::string>> pending;
    std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>> ranges;
    std::map<std::string, std::size_t> walk_counts;
    std::string line;
    std::size_t line_number = 0;
    while (reader.read(line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto f = fields(line);
        if (f.empty()) continue;
        if (f[0] == "H") {
            for (std::size_t i = 1; i < f.size(); ++i) if (f[i].starts_with("VN:Z:")) result.graph.version = f[i].substr(5);
        } else if (f[0] == "S" && f.size() >= 3) {
            const std::string name(f[1]); const auto sequence = f[2];
            const bool duplicate = result.graph.segments.contains(name);
            if (duplicate) result.errors.push_back("DUPLICATE_SEGMENT identifier=" + name);
            pending.erase(name);
            if (sequence == "*") result.errors.push_back("SEQUENCELESS_SEGMENT line=" + std::to_string(line_number) + " identifier=" + name);
            else {
                const auto invalid = std::find_if(sequence.begin(), sequence.end(), [](unsigned char c) { return !is_iupac(c); });
                if (invalid != sequence.end()) result.errors.push_back("INVALID_NUCLEOTIDE line=" + std::to_string(line_number) + " symbol=" + std::string(1, *invalid));
                else if (!duplicate) result.graph.segments.insert(name, result.graph.segment_spool->append({sequence.data(), sequence.size()}));
            }
        } else if (f[0] == "P" && f.size() >= 4) {
            const std::string name(f[1]); ++result.graph.path_record_count; result.graph.path_names.insert(name);
            if (!p_names.insert(name).second) result.errors.push_back("DUPLICATE_PATH identifier=" + name);
            auto steps = path_steps(f[2]);
            if (!steps) { result.errors.push_back("MALFORMED_PATH line=" + std::to_string(line_number)); steps = std::vector<Step>{}; }
            bool blunt = f[3] == "*";
            if (!blunt && steps->size() > 1) {
                std::size_t start = 0, count = 0; blunt = true;
                while (start <= f[3].size()) {
                    const auto end = f[3].find(',', start); ++count;
                    blunt &= f[3].substr(start, end - start) == "0M";
                    if (end == std::string_view::npos) break;
                    start = end + 1;
                }
                blunt &= count == steps->size() - 1;
            }
            if (!blunt) result.errors.push_back("UNSUPPORTED_OVERLAP line=" + std::to_string(line_number) + " overlap=" + std::string(f[3]));
            result.graph.unspecified_overlaps_as_blunt |= f[3] == "*";
            references(name, *steps, pending, result.graph.segments);
            const auto stored = result.graph.traversal_spool->append({f[2].data(), f[2].size()});
            result.graph.records[name] = {{RecordType::path, stored, {}, {}}};
        } else if (f[0] == "W" && f.size() >= 7) {
            ++result.graph.walk_record_count;
            if (!number(f[2])) result.errors.push_back("INVALID_HAPLOTYPE line=" + std::to_string(line_number) + " value=" + std::string(f[2]));
            const auto name = std::string(f[1]) + "#" + std::string(f[2]) + "#" + std::string(f[3]);
            result.graph.path_names.insert(name); walk_names.insert(name); ++walk_counts[name];
            auto steps = parsed_walk(f[6]);
            if (!steps) { result.errors.push_back("MALFORMED_WALK line=" + std::to_string(line_number)); steps = std::vector<Step>{}; }
            references(name, *steps, pending, result.graph.segments);
            std::optional<std::size_t> start, end;
            if (f[4] == "*" && f[5] == "*") unspecified.insert(name);
            else if ((start = number(f[4])) && (end = number(f[5])) && *start < *end) {
                ranges[name].push_back({*start, *end});
                if (auto source = source_lengths.find(name); source != source_lengths.end() && *end > source->second)
                    result.errors.push_back("W_RANGE_OUT_OF_BOUNDS identifier=" + name + " end=" + std::to_string(*end) + " source_length=" + std::to_string(source->second));
            } else { result.errors.push_back("INVALID_W_RANGE line=" + std::to_string(line_number)); start.reset(); end.reset(); }
            const auto stored = result.graph.traversal_spool->append({f[6].data(), f[6].size()});
            result.graph.records[name].push_back({RecordType::walk, stored, start, end});
        } else if (f[0] == "W") result.errors.push_back("MALFORMED_W_RECORD line=" + std::to_string(line_number));
    }
    for (const auto& name : p_names) if (walk_names.contains(name)) result.errors.push_back("DUPLICATE_EMBEDDED_PATH identifier=" + name);
    for (const auto& name : unspecified) if (walk_counts[name] > 1) result.errors.push_back("AMBIGUOUS_W_RANGES identifier=" + name);
    for (auto& [name, values] : ranges) {
        std::sort(values.begin(), values.end());
        for (std::size_t i = 1; i < values.size(); ++i) if (values[i].first < values[i - 1].second) result.errors.push_back("W_RANGE_OVERLAP identifier=" + name);
    }
    for (auto& [name, records] : result.graph.records) std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    for (const auto& [segment, paths] : pending) for (const auto& name : paths) result.errors.push_back("UNRESOLVED_SEGMENT path=" + name + " segment=" + segment);
    return result;
}

std::vector<Step> read_steps(const Graph& graph, const TraversalRecord& record) {
    const auto encoded = graph.traversal_spool->read(record.steps);
    auto parsed = record.type == RecordType::path ? path_steps(encoded) : parsed_walk(encoded);
    if (!parsed) throw std::runtime_error("malformed indexed traversal");
    return std::move(*parsed);
}
}
