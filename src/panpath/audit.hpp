#pragma once

#include "panpath/cancellation.hpp"
#include "panpath/digests.hpp"
#include "panpath/graph.hpp"
#include "panpath/input.hpp"
#include "panpath/memory.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace panpath {

enum class StatisticsMode { basic, comprehensive };
enum class Status { identical, divergent, missing_path, missing_source };
enum class AlignmentStatus { not_applicable, not_required, complete, limit_exceeded, memory_limit_exceeded };

struct ResourcePolicy {
    std::size_t threads = 1;
    std::uint64_t memory_bytes = 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t alignment_max_cells = 10'000'000;
    std::filesystem::path temp_dir;
};

struct AuditRequest {
    std::vector<SourceSpec> sources;
    std::filesystem::path gfa_path;
    StatisticsMode statistics = StatisticsMode::basic;
    ResourcePolicy resources;
};

struct BaseStatistics {
    std::uint64_t matched = 0, substituted = 0, source_only = 0, graph_only = 0;
    std::uint64_t not_embedded = 0, missing_path = 0, missing_source = 0;
    std::uint64_t unaligned_divergent_source = 0, unaligned_divergent_graph = 0;
    void add(const BaseStatistics& other);
};

struct OutcomeStatistics {
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
    std::size_t traversal_position, segment_position;
};

struct Outcome {
    std::string identifier;
    std::optional<std::size_t> source_index;
    Status status;
    std::optional<RecordType> record_type;
    std::size_t source_length = 0, graph_length = 0;
    std::optional<std::size_t> divergence;
    std::optional<Location> location;
    std::optional<std::pair<std::size_t, std::size_t>> source_range, range_lengths;
    std::optional<std::size_t> range_mismatch_position;
    std::size_t addressed_source_bases = 0, graph_ambiguous_bases = 0;
    std::optional<SequenceDigest> source_digest, addressed_source_digest, graph_digest;
    std::optional<OutcomeStatistics> statistics;
};

struct SummaryCounts {
    std::uint64_t identical = 0, divergent = 0, missing_path = 0, missing_source = 0;
};

struct Aggregate {
    std::uint64_t source_sequences = 0, graph_traversals = 0;
    std::uint64_t source_bases = 0, graph_bases = 0;
    std::uint64_t source_ambiguous = 0, graph_ambiguous = 0, ambiguous_columns = 0;
    std::uint64_t edit_distance = 0, aligned_columns = 0;
    std::uint64_t completed_alignments = 0, unavailable_alignments = 0;
    BaseStatistics bases;
    std::map<Status, std::uint64_t> statuses;
    void add(const Outcome& outcome);
};

struct AggregateStatistics {
    Aggregate global;
    std::vector<Aggregate> by_source;
    std::map<RecordType, Aggregate> by_record_type;
};

struct AuditResult {
    std::vector<Outcome> outcomes;
    SummaryCounts summary;
    std::optional<AggregateStatistics> statistics;
    MemoryTelemetry memory;
};

AuditResult audit(const AuditRequest& request, const Graph& graph,
                  Cancellation& cancellation,
                  const ProgressCallback& progress = {});

}
