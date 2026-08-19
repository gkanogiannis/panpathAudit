#pragma once

#include "panpath/cli.hpp"
#include "panpath/digests.hpp"
#include "panpath/graph.hpp"
#include "panpath/memory.hpp"
#include "panpath/statistics.hpp"

#include <optional>
#include <string>
#include <vector>

namespace panpath {

enum class Status { identical, divergent, missing_path, missing_source };
enum class AlignmentStatus { not_applicable, not_required, complete, limit_exceeded, memory_limit_exceeded };

struct BaseStatistics {
    std::uint64_t matched = 0, substituted = 0, source_only = 0, graph_only = 0;
    std::uint64_t not_embedded = 0, missing_path = 0, missing_source = 0;
    std::uint64_t unaligned_divergent_source = 0, unaligned_divergent_graph = 0;
};

struct OutcomeStatistics {
    std::optional<std::size_t> source_index;
    std::uint64_t source_bases = 0, graph_bases = 0;
    std::uint64_t source_ambiguous_bases = 0, graph_ambiguous_bases = 0, ambiguous_columns = 0;
    std::optional<std::int64_t> length_delta;
    AlignmentStatus alignment_status = AlignmentStatus::not_applicable;
    std::optional<std::uint64_t> edit_distance;
    BaseStatistics bases;
};

struct Location {
    std::string segment;
    Orientation orientation;
    std::size_t traversal_position;
    std::size_t segment_position;
};

struct Outcome {
    std::string identifier;
    Status status;
    std::optional<RecordType> record_type;
    std::size_t source_length = 0;
    std::size_t graph_length = 0;
    std::optional<std::size_t> divergence;
    std::optional<Location> location;
    std::optional<std::pair<std::size_t, std::size_t>> source_range;
    std::optional<std::pair<std::size_t, std::size_t>> range_lengths;
    std::optional<std::size_t> range_mismatch_position;
    std::size_t addressed_source_bases = 0;
    std::size_t graph_ambiguous_bases = 0;
    std::optional<SequenceDigest> source_digest;
    std::optional<SequenceDigest> addressed_source_digest;
    std::optional<SequenceDigest> graph_digest;
    std::optional<OutcomeStatistics> statistics;
};

struct AuditResult { std::vector<Outcome> outcomes; MemoryTelemetry memory; };

AuditResult audit_basic(const Config& config, const Graph& graph);

}
