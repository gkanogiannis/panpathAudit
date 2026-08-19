#include "panpath/report.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <map>
#include <sstream>
#include <string_view>

namespace panpath {
namespace {
using Json = nlohmann::json;
static constexpr const char* TSV_HEADER = "row_type\tidentifier\tstatus\trecord_type\tcode\tmessage\tsource_start_0based\tsource_end_0based_exclusive\tsource_length\tgraph_length\tdivergence_kind\tsource_position_1based\tgraph_position_1based\tsegment\torientation\ttraversal_position_1based\tsegment_position_1based\tsource_sha256\taddressed_source_sha256\tgraph_sha256\tsource_blake3\taddressed_source_blake3\tgraph_blake3\tmatched\tsubstituted\tsource_only\tgraph_only\tnot_embedded\tmissing_path_bases\tmissing_source_bases\tunaligned_divergent_source\tunaligned_divergent_graph\tedit_distance\talignment_status\ttopology_validated\tunspecified_overlaps_as_blunt";
const char* record_name(RecordType type) { return type == RecordType::path ? "P" : "W"; }
const char* status_name(Status status) {
    switch (status) {
        case Status::identical: return "IDENTICAL";
        case Status::divergent: return "DIVERGENT";
        case Status::missing_path: return "MISSING_PATH";
        case Status::missing_source: return "MISSING_SOURCE";
    }
    return "";
}
const char* alignment_name(AlignmentStatus status) {
    switch (status) {
        case AlignmentStatus::not_applicable: return "not_applicable";
        case AlignmentStatus::not_required: return "not_required";
        case AlignmentStatus::complete: return "complete";
        case AlignmentStatus::limit_exceeded: return "limit_exceeded";
        case AlignmentStatus::memory_limit_exceeded: return "memory_limit_exceeded";
    }
    return "";
}
Json percentage(std::uint64_t numerator, std::uint64_t denominator) {
    if (!denominator) return nullptr;
    return std::round(static_cast<double>(numerator) * 100000000.0 / static_cast<double>(denominator)) / 1000000.0;
}
void attach_statistics(Json& item, const OutcomeStatistics& value) {
    item["base_statistics"] = {
        {"source", {{"matched", value.bases.matched}, {"substituted", value.bases.substituted},
                    {"source_only", value.bases.source_only}, {"not_embedded", value.bases.not_embedded},
                    {"missing_path", value.bases.missing_path}, {"unaligned_divergent", value.bases.unaligned_divergent_source}}},
        {"graph", {{"matched", value.bases.matched}, {"substituted", value.bases.substituted},
                   {"graph_only", value.bases.graph_only}, {"missing_source", value.bases.missing_source},
                   {"unaligned_divergent", value.bases.unaligned_divergent_graph}}}
    };
    item["alignment_status"] = alignment_name(value.alignment_status);
    item["edit_distance"] = value.edit_distance ? Json(*value.edit_distance) : Json(nullptr);
    item["length_delta"] = value.length_delta ? Json(*value.length_delta) : Json(nullptr);
    item["ambiguity"] = {{"source_bases", value.source_ambiguous_bases},
                         {"graph_bases", value.graph_ambiguous_bases}, {"aligned_columns", value.ambiguous_columns}};
}

struct Accumulator {
    std::uint64_t source_sequences = 0, graph_traversals = 0, source_bases = 0, graph_bases = 0;
    std::uint64_t source_ambiguous = 0, graph_ambiguous = 0, ambiguous_columns = 0;
    std::uint64_t edit_distance = 0, aligned_columns = 0, completed = 0, unavailable = 0;
    BaseStatistics bases;
    void add(const OutcomeStatistics& value) {
        source_sequences += value.source_index.has_value();
        graph_traversals += value.graph_bases > 0 || !value.source_index;
        source_bases += value.source_bases; graph_bases += value.graph_bases;
        source_ambiguous += value.source_ambiguous_bases; graph_ambiguous += value.graph_ambiguous_bases;
        ambiguous_columns += value.ambiguous_columns;
        bases.matched += value.bases.matched; bases.substituted += value.bases.substituted;
        bases.source_only += value.bases.source_only; bases.graph_only += value.bases.graph_only;
        bases.not_embedded += value.bases.not_embedded; bases.missing_path += value.bases.missing_path;
        bases.missing_source += value.bases.missing_source;
        bases.unaligned_divergent_source += value.bases.unaligned_divergent_source;
        bases.unaligned_divergent_graph += value.bases.unaligned_divergent_graph;
        if (value.edit_distance) {
            edit_distance += *value.edit_distance;
            aligned_columns += value.bases.matched + value.bases.substituted + value.bases.source_only + value.bases.graph_only;
        }
        completed += value.alignment_status == AlignmentStatus::complete;
        unavailable += value.alignment_status == AlignmentStatus::limit_exceeded || value.alignment_status == AlignmentStatus::memory_limit_exceeded;
    }
};
Json accumulator_json(const Accumulator& value) {
    const auto embedded = value.source_bases - value.bases.not_embedded - value.bases.missing_path;
    const auto sourced_graph = value.graph_bases - value.bases.missing_source;
    return {
        {"source", {{"sequences", value.source_sequences}, {"total_bases", value.source_bases},
            {"bases", {{"matched", value.bases.matched}, {"substituted", value.bases.substituted}, {"source_only", value.bases.source_only},
                       {"not_embedded", value.bases.not_embedded}, {"missing_path", value.bases.missing_path}, {"unaligned_divergent", value.bases.unaligned_divergent_source}}},
            {"percentages", {{"matched", percentage(value.bases.matched, value.source_bases)}, {"substituted", percentage(value.bases.substituted, value.source_bases)},
                             {"source_only", percentage(value.bases.source_only, value.source_bases)}, {"not_embedded", percentage(value.bases.not_embedded, value.source_bases)},
                             {"missing_path", percentage(value.bases.missing_path, value.source_bases)}, {"unaligned_divergent", percentage(value.bases.unaligned_divergent_source, value.source_bases)}}},
            {"embedded_coverage_percent", percentage(embedded, value.source_bases)}, {"exact_recovery_percent", percentage(value.bases.matched, value.source_bases)}}},
        {"graph", {{"traversals", value.graph_traversals}, {"total_bases", value.graph_bases},
            {"bases", {{"matched", value.bases.matched}, {"substituted", value.bases.substituted}, {"graph_only", value.bases.graph_only},
                       {"missing_source", value.bases.missing_source}, {"unaligned_divergent", value.bases.unaligned_divergent_graph}}},
            {"percentages", {{"matched", percentage(value.bases.matched, value.graph_bases)}, {"substituted", percentage(value.bases.substituted, value.graph_bases)},
                             {"graph_only", percentage(value.bases.graph_only, value.graph_bases)}, {"missing_source", percentage(value.bases.missing_source, value.graph_bases)},
                             {"unaligned_divergent", percentage(value.bases.unaligned_divergent_graph, value.graph_bases)}}},
            {"source_coverage_percent", percentage(sourced_graph, value.graph_bases)}}},
        {"alignment", {{"identity_percent", percentage(value.bases.matched, value.aligned_columns)}, {"edit_distance", value.edit_distance},
                       {"completed_traversals", value.completed}, {"unavailable_traversals", value.unavailable}, {"aligned_columns", value.aligned_columns}}},
        {"ambiguity", {{"source_bases", value.source_ambiguous}, {"graph_bases", value.graph_ambiguous}, {"aligned_columns", value.ambiguous_columns}}}
    };
}
Json statuses_json(const std::map<std::string, std::uint64_t>& counts, std::uint64_t denominator,
                   std::initializer_list<const char*> names) {
    Json result = Json::object();
    for (const auto* name : names) {
        const auto found = counts.find(name); const auto count = found == counts.end() ? 0 : found->second;
        result[name] = {{"count", count}, {"percent", percentage(count, denominator)}};
    }
    return result;
}
const char* status_key(Status status) {
    switch (status) {
        case Status::identical: return "identical";
        case Status::divergent: return "divergent";
        case Status::missing_path: return "missing_path";
        case Status::missing_source: return "missing_source";
    }
    return "unknown";
}
Json digest_value(const std::optional<SequenceDigest>& value, bool sha) {
    return value ? Json(sha ? value->sha256 : value->blake3) : Json(nullptr);
}
void attach_digests(Json& outcome, const Outcome& value) {
    outcome["digests"] = {
        {"sha256", {{"source", digest_value(value.source_digest, true)},
                    {"addressed_source", digest_value(value.addressed_source_digest, true)},
                    {"graph", digest_value(value.graph_digest, true)}}},
        {"blake3", {{"source", digest_value(value.source_digest, false)},
                    {"addressed_source", digest_value(value.addressed_source_digest, false)},
                    {"graph", digest_value(value.graph_digest, false)}}}
    };
}
}

std::string render_json(const Config& config, const Graph& graph,
                        const SourceIndex& sources, const AuditResult& audit) {
    Json outcomes = Json::array();
    std::size_t identical = 0, divergent = 0, missing_path = 0, missing_source = 0;
    for (const auto& value : audit.outcomes) {
        identical += value.status == Status::identical; divergent += value.status == Status::divergent;
        missing_path += value.status == Status::missing_path; missing_source += value.status == Status::missing_source;
        Json outcome = {{"identifier", value.identifier}, {"status", status_name(value.status)}};
        if (value.record_type) outcome["record_type"] = record_name(*value.record_type);
        if (value.status == Status::identical || value.status == Status::divergent) {
            outcome["source_length"] = value.source_length; outcome["path_length"] = value.graph_length;
        }
        if (value.source_range) outcome["source_range"] = {{"start_0based", value.source_range->first},
                                                            {"end_0based_exclusive", value.source_range->second}};
        if (value.status == Status::divergent) {
            const auto position = *value.divergence + 1;
            outcome["source_position_1based"] = position; outcome["path_position_1based"] = position;
            Json detail = {{"source_position_1based", position}, {"path_position_1based", position}};
            if (value.range_mismatch_position && *value.range_mismatch_position == *value.divergence && value.range_lengths) {
                detail["kind"] = "RANGE_LENGTH";
                detail["range_length"] = value.range_lengths->first;
                detail["walk_length"] = value.range_lengths->second;
            } else if (value.location) {
                detail["segment"] = value.location->segment;
                detail["orientation"] = value.location->orientation == Orientation::forward ? "+" : "-";
                detail["traversal_position_1based"] = value.location->traversal_position;
                detail["segment_position_1based"] = value.location->segment_position;
            } else detail["graph_state"] = "END";
            outcome["divergence"] = std::move(detail);
        }
        if (value.statistics) attach_statistics(outcome, *value.statistics);
        attach_digests(outcome, value); outcomes.push_back(std::move(outcome));
    }
    Json source_paths = Json::array(), source_details = Json::array();
    for (const auto& source : config.sources) {
        source_paths.push_back(source.path.string());
        Json detail = {{"path", source.path.string()}};
        if (source.mode == SourceMode::exact) detail["mapping"] = "exact";
        else if (source.mode == SourceMode::pansn) { detail["mapping"] = "pansn"; detail["sample"] = source.sample; detail["haplotype"] = source.haplotype; }
        else { detail["mapping"] = "prefix"; detail["prefix"] = source.prefix; }
        source_details.push_back(std::move(detail));
    }
    Json record_types = Json::array();
    if (graph.path_record_count) record_types.push_back("P");
    if (graph.walk_record_count) record_types.push_back("W");
    Json provenance = {
        {"tool_version", PANPATH_VERSION}, {"source_path", config.sources.size() == 1 ? Json(config.sources[0].path.string()) : Json(nullptr)},
        {"source_paths", source_paths}, {"sources", source_details}, {"graph_path", config.gfa_path.string()},
        {"fasta_compression", sources.compressions.size() == 1 ? Json(sources.compressions[0]) : Json(nullptr)},
        {"fasta_compressions", sources.compressions}, {"gfa_version", graph.version}, {"gfa_compression", graph.compression},
        {"path_records", graph.path_record_count}, {"walk_records", graph.walk_record_count}, {"record_types", record_types},
        {"unspecified_overlaps_as_blunt", graph.unspecified_overlaps_as_blunt}, {"topology_validated", false},
        {"threads", config.threads}, {"memory_mib", audit.memory.limit_bytes / (1024 * 1024)},
        {"peak_tracked_bytes", audit.memory.peak_tracked_bytes}, {"oversized_contigs", audit.memory.oversized_contigs}
    };
    Json document = {{"schema", "panpath-audit-report"}, {"schema_version", PANPATH_VERSION}, {"state", "completed"},
                     {"summary", {{"identical", identical}, {"divergent", divergent}, {"missing_path", missing_path}, {"missing_source", missing_source}}},
                     {"outcomes", outcomes}, {"errors", Json::array()}, {"provenance", provenance}};
    if (config.comprehensive) {
        Accumulator global;
        std::vector<Accumulator> by_source(config.sources.size());
        std::map<RecordType, Accumulator> by_type;
        std::map<std::string, std::uint64_t> global_statuses;
        std::vector<std::map<std::string, std::uint64_t>> source_statuses(config.sources.size());
        std::map<RecordType, std::map<std::string, std::uint64_t>> type_statuses;
        for (const auto& outcome : audit.outcomes) if (outcome.statistics) {
            global.add(*outcome.statistics);
            ++global_statuses[status_key(outcome.status)];
            if (outcome.statistics->source_index) {
                const auto index = *outcome.statistics->source_index;
                by_source[index].add(*outcome.statistics); ++source_statuses[index][status_key(outcome.status)];
            }
            if (outcome.record_type) {
                by_type[*outcome.record_type].add(*outcome.statistics);
                ++type_statuses[*outcome.record_type][status_key(outcome.status)];
            }
        }
        Json statistics = accumulator_json(global);
        statistics["source"]["statuses"] = statuses_json(global_statuses, global.source_sequences,
            {"identical", "divergent", "missing_path"});
        statistics["graph"]["statuses"] = statuses_json(global_statuses, global.graph_traversals,
            {"identical", "divergent", "missing_source"});
        statistics["by_source"] = Json::array();
        for (std::size_t index = 0; index < by_source.size(); ++index) {
            auto item = accumulator_json(by_source[index]); item["source_index"] = index;
            item["source"]["statuses"] = statuses_json(source_statuses[index], by_source[index].source_sequences,
                {"identical", "divergent", "missing_path"});
            item["path"] = config.sources[index].path.string(); statistics["by_source"].push_back(std::move(item));
        }
        statistics["by_record_type"] = Json::object();
        for (const auto& [type, value] : by_type) {
            auto item = accumulator_json(value);
            item["graph"]["statuses"] = statuses_json(type_statuses[type], value.graph_traversals,
                {"identical", "divergent", "missing_source"});
            statistics["by_record_type"][record_name(type)] = std::move(item);
        }
        document["statistics"] = std::move(statistics);
        document["provenance"]["statistics"] = {{"mode", "comprehensive"},
            {"alignment", "exact_unit_cost_levenshtein_wavefront"}, {"alignment_max_cells", config.alignment_max_cells}};
    }
    return document.dump();
}

std::string render_tsv(const Graph& graph, const AuditResult& audit) {
    std::ostringstream output; output << TSV_HEADER << '\n';
    for (const auto& value : audit.outcomes) {
        std::vector<std::string> row(36);
        row[0] = "outcome"; row[1] = value.identifier; row[2] = status_name(value.status);
        if (value.record_type) row[3] = record_name(*value.record_type);
        if (value.source_range) { row[6] = std::to_string(value.source_range->first); row[7] = std::to_string(value.source_range->second); }
        if (value.status == Status::identical || value.status == Status::divergent) {
            row[8] = std::to_string(value.source_length); row[9] = std::to_string(value.graph_length);
        }
        if (value.divergence) {
            const bool range = value.range_mismatch_position && *value.range_mismatch_position == *value.divergence && value.range_lengths;
            row[10] = range ? "RANGE_LENGTH" : value.location ? "BASE" : "END"; row[11] = row[12] = std::to_string(*value.divergence + 1);
            if (range) row[13] = "range=" + std::to_string(value.range_lengths->first) + " walk=" + std::to_string(value.range_lengths->second);
            else if (value.location) {
                row[13] = value.location->segment; row[14] = value.location->orientation == Orientation::forward ? "+" : "-";
                row[15] = std::to_string(value.location->traversal_position); row[16] = std::to_string(value.location->segment_position);
            }
        }
        if (value.source_digest) { row[17] = value.source_digest->sha256; row[20] = value.source_digest->blake3; }
        if (value.addressed_source_digest) { row[18] = value.addressed_source_digest->sha256; row[21] = value.addressed_source_digest->blake3; }
        if (value.graph_digest) { row[19] = value.graph_digest->sha256; row[22] = value.graph_digest->blake3; }
        if (value.statistics) {
            const auto& s = *value.statistics;
            row[23] = std::to_string(s.bases.matched); row[24] = std::to_string(s.bases.substituted);
            row[25] = std::to_string(s.bases.source_only); row[26] = std::to_string(s.bases.graph_only);
            row[27] = std::to_string(s.bases.not_embedded); row[28] = std::to_string(s.bases.missing_path);
            row[29] = std::to_string(s.bases.missing_source); row[30] = std::to_string(s.bases.unaligned_divergent_source);
            row[31] = std::to_string(s.bases.unaligned_divergent_graph);
            if (s.edit_distance) row[32] = std::to_string(*s.edit_distance);
            row[33] = alignment_name(s.alignment_status);
        }
        row[34] = "false"; row[35] = graph.unspecified_overlaps_as_blunt ? "true" : "false";
        for (std::size_t i = 0; i < row.size(); ++i) { if (i) output << '\t'; output << row[i]; }
        output << '\n';
    }
    return output.str();
}

std::string render_invalid_json(const std::vector<std::string>& source_errors,
                                const std::vector<std::string>& graph_errors,
                                const std::vector<std::string>& operational_errors,
                                bool comprehensive) {
    Json errors = Json::array();
    const auto append = [&](const std::vector<std::string>& values, const char* category) {
        for (const auto& message : values) {
            const auto end = message.find_first_of(" \t");
            const auto code = message.substr(0, end);
            const bool correspondence = category == std::string_view("gfa") &&
                (code == "DUPLICATE_PATH" || code == "DUPLICATE_EMBEDDED_PATH" ||
                 code == "AMBIGUOUS_W_RANGES" || code == "W_RANGE_OVERLAP");
            errors.push_back({{"category", correspondence ? "correspondence" : category},
                              {"code", code.empty() ? "ERROR" : code}, {"message", message}});
        }
    };
    append(source_errors, "source"); append(graph_errors, "gfa"); append(operational_errors, "operational");
    Json document = {{"schema", "panpath-audit-report"}, {"schema_version", PANPATH_VERSION}, {"state", "invalid"},
        {"summary", {{"identical", 0}, {"divergent", 0}, {"missing_path", 0}, {"missing_source", 0}}},
        {"outcomes", Json::array()}, {"errors", std::move(errors)}, {"provenance", {{"topology_validated", false}}}};
    if (comprehensive) document["statistics"] = nullptr;
    return document.dump();
}

std::string render_invalid_tsv(const std::vector<std::string>& source_errors,
                               const std::vector<std::string>& graph_errors,
                               const std::vector<std::string>& operational_errors) {
    std::ostringstream output; output << TSV_HEADER << '\n';
    const auto append = [&](const std::vector<std::string>& values) {
        for (auto message : values) {
            std::replace(message.begin(), message.end(), '\t', ' ');
            const auto end = message.find(' '); const auto code = message.substr(0, end);
            std::vector<std::string> row(36); row[0] = "error"; row[4] = code.empty() ? "ERROR" : code; row[5] = message;
            for (std::size_t index = 0; index < row.size(); ++index) { if (index) output << '\t'; output << row[index]; }
            output << '\n';
        }
    };
    append(source_errors); append(graph_errors); append(operational_errors);
    return output.str();
}
}
