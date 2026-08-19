#pragma once

#include "panpath/sequence.hpp"
#include "panpath/spool.hpp"

#include <filesystem>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace panpath {

enum class RecordType { path, walk };

struct Step { std::string segment; Orientation orientation; };
struct TraversalRecord {
    RecordType type;
    RecordRef steps;
    std::optional<std::size_t> start;
    std::optional<std::size_t> end;
};

class SegmentIndex {
public:
    bool contains(const std::string& name) const;
    bool insert(const std::string& name, RecordRef record);
    RecordRef at(const std::string& name) const;
private:
    static std::optional<std::size_t> numeric_name(const std::string& name);
    static std::array<std::uint8_t, 12> pack(RecordRef record);
    static RecordRef unpack(const std::array<std::uint8_t, 12>& record);
    static bool missing(const std::array<std::uint8_t, 12>& record);
    std::vector<std::array<std::uint8_t, 12>> numeric_{1};
    std::map<std::string, RecordRef> named_;
};

struct Graph {
    SegmentIndex segments;
    std::shared_ptr<Spool> segment_spool;
    std::shared_ptr<Spool> traversal_spool;
    std::map<std::string, std::vector<TraversalRecord>> records;
    std::set<std::string> path_names;
    std::string version = "inferred-1.x";
    std::string compression;
    std::size_t path_record_count = 0;
    std::size_t walk_record_count = 0;
    bool unspecified_overlaps_as_blunt = false;
};

struct GraphReadResult {
    Graph graph;
    std::vector<std::string> errors;
};

GraphReadResult read_graph(const std::filesystem::path& path,
                           const std::map<std::string, std::size_t>& source_lengths,
                           const std::filesystem::path& temp_dir);
std::vector<Step> read_steps(const Graph& graph, const TraversalRecord& record);

}
