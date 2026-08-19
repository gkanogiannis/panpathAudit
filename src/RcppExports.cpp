#include <Rcpp.h>
#include <R_ext/Rdynload.h>

#include "panpath/engine.hpp"
#include "panpath/spool.hpp"
#include "panpath/statistics.hpp"

#include <filesystem>
#include <string>
#include <thread>

namespace {

using namespace Rcpp;
using namespace panpath;

std::filesystem::path path_from_r(SEXP value) {
    const auto text = as<std::string>(value);
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string path_to_r(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}

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

const char* record_name(RecordType type) {
    return type == RecordType::path ? "P" : "W";
}

AuditRequest request_from_r(const List& input) {
    AuditRequest request;
    const List sources = input["sources"];
    request.sources.reserve(sources.size());
    for (SEXP value : sources) {
        const List item(value);
        SourceSpec source;
        source.path = path_from_r(item["path"]);
        const auto mode = as<std::string>(item["mode"]);
        if (mode == "pansn") source.mode = SourceMode::pansn;
        else if (mode == "prefix") source.mode = SourceMode::prefix;
        source.sample = as<std::string>(item["sample"]);
        source.haplotype = as<std::string>(item["haplotype"]);
        source.prefix = as<std::string>(item["prefix"]);
        request.sources.push_back(std::move(source));
    }
    request.gfa_path = path_from_r(input["gfa"]);
    request.statistics = as<std::string>(input["statistics"]) == "comprehensive"
        ? StatisticsMode::comprehensive : StatisticsMode::basic;
    request.resources.threads = static_cast<std::size_t>(as<double>(input["threads"]));
    request.resources.memory_bytes = static_cast<std::uint64_t>(as<double>(input["memoryBytes"]));
    request.resources.alignment_max_cells =
        static_cast<std::uint64_t>(as<double>(input["alignmentMaxCells"]));
    request.resources.temp_dir = path_from_r(input["tempDir"]);
    return request;
}

DataFrame outcomes_to_r(const std::vector<Outcome>& outcomes) {
    const auto size = static_cast<R_xlen_t>(outcomes.size());
    CharacterVector identifier(size), status(size), record_type(size, NA_STRING);
    CharacterVector divergence_kind(size, NA_STRING), segment(size, NA_STRING);
    CharacterVector orientation(size, NA_STRING), alignment_status(size, NA_STRING);
    NumericVector source_index(size, NA_REAL), source_start(size, NA_REAL), source_end(size, NA_REAL);
    NumericVector source_length(size, NA_REAL), graph_length(size, NA_REAL);
    NumericVector source_position(size, NA_REAL), graph_position(size, NA_REAL);
    NumericVector traversal_position(size, NA_REAL), segment_position_value(size, NA_REAL);
    NumericVector range_length(size, NA_REAL), walk_length(size, NA_REAL);
    NumericVector addressed_source_bases(size, NA_REAL), source_ambiguous(size, NA_REAL);
    NumericVector graph_ambiguous(size, NA_REAL), ambiguous_columns(size, NA_REAL);
    NumericVector length_delta(size, NA_REAL), matched(size, NA_REAL), substituted(size, NA_REAL);
    NumericVector source_only(size, NA_REAL), graph_only(size, NA_REAL), not_embedded(size, NA_REAL);
    NumericVector missing_path(size, NA_REAL), missing_source(size, NA_REAL);
    NumericVector unaligned_source(size, NA_REAL), unaligned_graph(size, NA_REAL);
    NumericVector edit_distance(size, NA_REAL);
    CharacterVector source_sha(size, NA_STRING), addressed_sha(size, NA_STRING), graph_sha(size, NA_STRING);
    CharacterVector source_blake(size, NA_STRING), addressed_blake(size, NA_STRING), graph_blake(size, NA_STRING);

    for (R_xlen_t i = 0; i < size; ++i) {
        const auto& value = outcomes[static_cast<std::size_t>(i)];
        identifier[i] = value.identifier;
        status[i] = status_name(value.status);
        if (value.source_index) source_index[i] = static_cast<double>(*value.source_index + 1);
        if (value.record_type) record_type[i] = record_name(*value.record_type);
        if (value.source_range) {
            source_start[i] = static_cast<double>(value.source_range->first);
            source_end[i] = static_cast<double>(value.source_range->second);
        }
        if (value.source_index) source_length[i] = static_cast<double>(value.source_length);
        if (value.record_type) graph_length[i] = static_cast<double>(value.graph_length);
        addressed_source_bases[i] = static_cast<double>(value.addressed_source_bases);
        graph_ambiguous[i] = static_cast<double>(value.graph_ambiguous_bases);
        if (value.divergence) {
            source_position[i] = graph_position[i] = static_cast<double>(*value.divergence + 1);
            const bool range = value.range_mismatch_position &&
                *value.range_mismatch_position == *value.divergence && value.range_lengths;
            divergence_kind[i] = range ? "RANGE_LENGTH" : value.location ? "BASE" : "END";
            if (range) {
                range_length[i] = static_cast<double>(value.range_lengths->first);
                walk_length[i] = static_cast<double>(value.range_lengths->second);
            } else if (value.location) {
                segment[i] = value.location->segment;
                orientation[i] = value.location->orientation == Orientation::forward ? "+" : "-";
                traversal_position[i] = static_cast<double>(value.location->traversal_position);
                segment_position_value[i] = static_cast<double>(value.location->segment_position);
            }
        }
        if (value.source_digest) {
            source_sha[i] = value.source_digest->sha256;
            source_blake[i] = value.source_digest->blake3;
        }
        if (value.addressed_source_digest) {
            addressed_sha[i] = value.addressed_source_digest->sha256;
            addressed_blake[i] = value.addressed_source_digest->blake3;
        }
        if (value.graph_digest) {
            graph_sha[i] = value.graph_digest->sha256;
            graph_blake[i] = value.graph_digest->blake3;
        }
        if (value.statistics) {
            const auto& statistics = *value.statistics;
            source_ambiguous[i] = static_cast<double>(statistics.source_ambiguous_bases);
            graph_ambiguous[i] = static_cast<double>(statistics.graph_ambiguous_bases);
            ambiguous_columns[i] = static_cast<double>(statistics.ambiguous_columns);
            if (statistics.length_delta) length_delta[i] = static_cast<double>(*statistics.length_delta);
            matched[i] = static_cast<double>(statistics.bases.matched);
            substituted[i] = static_cast<double>(statistics.bases.substituted);
            source_only[i] = static_cast<double>(statistics.bases.source_only);
            graph_only[i] = static_cast<double>(statistics.bases.graph_only);
            not_embedded[i] = static_cast<double>(statistics.bases.not_embedded);
            missing_path[i] = static_cast<double>(statistics.bases.missing_path);
            missing_source[i] = static_cast<double>(statistics.bases.missing_source);
            unaligned_source[i] = static_cast<double>(statistics.bases.unaligned_divergent_source);
            unaligned_graph[i] = static_cast<double>(statistics.bases.unaligned_divergent_graph);
            if (statistics.edit_distance) edit_distance[i] = static_cast<double>(*statistics.edit_distance);
            alignment_status[i] = alignment_name(statistics.alignment_status);
        }
    }

    return DataFrame::create(
        _["identifier"] = identifier, _["source_index"] = source_index,
        _["status"] = status, _["record_type"] = record_type,
        _["source_start_0based"] = source_start,
        _["source_end_0based_exclusive"] = source_end,
        _["source_length"] = source_length, _["graph_length"] = graph_length,
        _["addressed_source_bases"] = addressed_source_bases,
        _["divergence_kind"] = divergence_kind,
        _["source_position_1based"] = source_position,
        _["graph_position_1based"] = graph_position,
        _["segment"] = segment, _["orientation"] = orientation,
        _["traversal_position_1based"] = traversal_position,
        _["segment_position_1based"] = segment_position_value,
        _["range_length"] = range_length, _["walk_length"] = walk_length,
        _["source_sha256"] = source_sha, _["addressed_source_sha256"] = addressed_sha,
        _["graph_sha256"] = graph_sha, _["source_blake3"] = source_blake,
        _["addressed_source_blake3"] = addressed_blake, _["graph_blake3"] = graph_blake,
        _["source_ambiguous_bases"] = source_ambiguous,
        _["graph_ambiguous_bases"] = graph_ambiguous,
        _["ambiguous_columns"] = ambiguous_columns, _["length_delta"] = length_delta,
        _["matched"] = matched, _["substituted"] = substituted,
        _["source_only"] = source_only, _["graph_only"] = graph_only,
        _["not_embedded"] = not_embedded, _["missing_path_bases"] = missing_path,
        _["missing_source_bases"] = missing_source,
        _["unaligned_divergent_source"] = unaligned_source,
        _["unaligned_divergent_graph"] = unaligned_graph,
        _["edit_distance"] = edit_distance, _["alignment_status"] = alignment_status
    );
}

List base_statistics_to_r(const BaseStatistics& value) {
    return List::create(
        _["matched"] = static_cast<double>(value.matched),
        _["substituted"] = static_cast<double>(value.substituted),
        _["source_only"] = static_cast<double>(value.source_only),
        _["graph_only"] = static_cast<double>(value.graph_only),
        _["not_embedded"] = static_cast<double>(value.not_embedded),
        _["missing_path"] = static_cast<double>(value.missing_path),
        _["missing_source"] = static_cast<double>(value.missing_source),
        _["unaligned_divergent_source"] = static_cast<double>(value.unaligned_divergent_source),
        _["unaligned_divergent_graph"] = static_cast<double>(value.unaligned_divergent_graph)
    );
}

List statuses_to_r(const std::map<Status, std::uint64_t>& statuses) {
    const auto count = [&](Status status) {
        const auto found = statuses.find(status);
        return static_cast<double>(found == statuses.end() ? 0 : found->second);
    };
    return List::create(_["identical"] = count(Status::identical),
        _["divergent"] = count(Status::divergent),
        _["missing_path"] = count(Status::missing_path),
        _["missing_source"] = count(Status::missing_source));
}

List aggregate_to_r(const Aggregate& value) {
    return List::create(
        _["source_sequences"] = static_cast<double>(value.source_sequences),
        _["graph_traversals"] = static_cast<double>(value.graph_traversals),
        _["source_bases"] = static_cast<double>(value.source_bases),
        _["graph_bases"] = static_cast<double>(value.graph_bases),
        _["source_ambiguous_bases"] = static_cast<double>(value.source_ambiguous),
        _["graph_ambiguous_bases"] = static_cast<double>(value.graph_ambiguous),
        _["ambiguous_columns"] = static_cast<double>(value.ambiguous_columns),
        _["edit_distance"] = static_cast<double>(value.edit_distance),
        _["aligned_columns"] = static_cast<double>(value.aligned_columns),
        _["completed_alignments"] = static_cast<double>(value.completed_alignments),
        _["unavailable_alignments"] = static_cast<double>(value.unavailable_alignments),
        _["bases"] = base_statistics_to_r(value.bases),
        _["statuses"] = statuses_to_r(value.statuses)
    );
}

List statistics_to_r(const std::optional<AggregateStatistics>& statistics) {
    if (!statistics) return List::create();
    List by_source(statistics->by_source.size());
    for (R_xlen_t i = 0; i < by_source.size(); ++i)
        by_source[i] = aggregate_to_r(statistics->by_source[static_cast<std::size_t>(i)]);
    List by_record_type(statistics->by_record_type.size());
    CharacterVector names(statistics->by_record_type.size());
    R_xlen_t index = 0;
    for (const auto& [type, aggregate] : statistics->by_record_type) {
        by_record_type[index] = aggregate_to_r(aggregate);
        names[index] = record_name(type);
        ++index;
    }
    by_record_type.attr("names") = names;
    return List::create(_["global"] = aggregate_to_r(statistics->global),
        _["by_source"] = by_source, _["by_record_type"] = by_record_type);
}

List diagnostics_to_r(const std::vector<Diagnostic>& diagnostics) {
    List result(diagnostics.size());
    for (R_xlen_t i = 0; i < result.size(); ++i) {
        const auto& value = diagnostics[static_cast<std::size_t>(i)];
        String path = NA_STRING, identifier = NA_STRING, symbol = NA_STRING;
        if (value.path) path = path_to_r(*value.path);
        if (value.identifier) identifier = *value.identifier;
        if (value.symbol) symbol = *value.symbol;
        List details(value.details.size());
        CharacterVector names(value.details.size());
        R_xlen_t detail_index = 0;
        for (const auto& [name, detail] : value.details) {
            details[detail_index] = detail;
            names[detail_index] = name;
            ++detail_index;
        }
        details.attr("names") = names;
        result[i] = List::create(
            _["category"] = category_name(value.category),
            _["code"] = value.code, _["message"] = value.message,
            _["path"] = path,
            _["line"] = value.line ? static_cast<double>(*value.line) : NA_REAL,
            _["identifier"] = identifier,
            _["position"] = value.position ? static_cast<double>(*value.position) : NA_REAL,
            _["symbol"] = symbol,
            _["details"] = details
        );
    }
    return result;
}

List response_to_r(const EngineResponse& response) {
    if (!response.result) return List::create(_["state"] = "invalid",
        _["diagnostics"] = diagnostics_to_r(response.diagnostics));
    const auto& result = *response.result;
    const auto& provenance = *response.provenance;
    return List::create(
        _["state"] = "completed",
        _["outcomes"] = outcomes_to_r(result.outcomes),
        _["summary"] = List::create(
            _["identical"] = static_cast<double>(result.summary.identical),
            _["divergent"] = static_cast<double>(result.summary.divergent),
            _["missing_path"] = static_cast<double>(result.summary.missing_path),
            _["missing_source"] = static_cast<double>(result.summary.missing_source)),
        _["statistics"] = statistics_to_r(result.statistics),
        _["telemetry"] = List::create(
            _["limit_bytes"] = static_cast<double>(result.memory.limit_bytes),
            _["peak_tracked_bytes"] = static_cast<double>(result.memory.peak_tracked_bytes),
            _["oversized_contigs"] = static_cast<double>(result.memory.oversized_contigs)),
        _["native_provenance"] = List::create(
            _["fasta_compressions"] = provenance.fasta_compressions,
            _["gfa_version"] = provenance.gfa_version,
            _["gfa_compression"] = provenance.gfa_compression,
            _["path_records"] = static_cast<double>(provenance.path_records),
            _["walk_records"] = static_cast<double>(provenance.walk_records),
            _["unspecified_overlaps_as_blunt"] = provenance.unspecified_overlaps_as_blunt),
        _["diagnostics"] = diagnostics_to_r(response.diagnostics)
    );
}

}

extern "C" SEXP _panpathAudit_audit(SEXP request_sexp) {
    BEGIN_RCPP
    const Rcpp::List request(request_sexp);
    panpath::EngineResponse response;
    try {
        auto native_request = request_from_r(request);
        response = panpath::run_engine(native_request, [] {
            Rcpp::checkUserInterrupt();
        });
    } catch (const Rcpp::internal::InterruptedException&) {
        throw;
    } catch (const std::exception& error) {
        response.diagnostics.emplace_back(panpath::DiagnosticCategory::internal,
            "INTERNAL_ERROR", error.what());
    } catch (...) {
        response.diagnostics.emplace_back(panpath::DiagnosticCategory::internal,
            "INTERNAL_ERROR", "unknown native failure");
    }
    return response_to_r(response);
    END_RCPP
}

extern "C" SEXP _panpathAudit_align(SEXP source_sexp, SEXP graph_sexp, SEXP max_cells_sexp) {
    BEGIN_RCPP
    const auto source = Rcpp::as<std::string>(source_sexp);
    const auto graph = Rcpp::as<std::string>(graph_sexp);
    panpath::Cancellation cancellation;
    const auto result = panpath::align(
        {reinterpret_cast<const std::uint8_t*>(source.data()), source.size()},
        {reinterpret_cast<const std::uint8_t*>(graph.data()), graph.size()},
        static_cast<std::uint64_t>(Rcpp::as<double>(max_cells_sexp)), &cancellation);
    if (!result.counts) return R_NilValue;
    return Rcpp::List::create(
        Rcpp::_["matched"] = static_cast<double>(result.counts->matched),
        Rcpp::_["substituted"] = static_cast<double>(result.counts->substituted),
        Rcpp::_["source_only"] = static_cast<double>(result.counts->source_only),
        Rcpp::_["graph_only"] = static_cast<double>(result.counts->graph_only),
        Rcpp::_["edit_distance"] = static_cast<double>(result.counts->edit_distance()),
        Rcpp::_["cells"] = static_cast<double>(result.cells));
    END_RCPP
}

extern "C" SEXP _panpathAudit_spool_test(SEXP directory_sexp) {
    BEGIN_RCPP
    std::filesystem::path path;
    {
        panpath::Spool spool(path_from_r(directory_sexp));
        path = spool.path();
        const std::string first = "ACGT", second = "TGCA";
        const auto left = spool.append({first.data(), first.size()});
        const auto right = spool.append({second.data(), second.size()});
        std::string left_value, right_value;
        std::thread left_reader([&] { left_value = spool.read(left); });
        std::thread right_reader([&] { right_value = spool.read(right); });
        left_reader.join();
        right_reader.join();
        if (left_value != first || right_value != second)
            Rcpp::stop("spool round trip failed");
    }
    return Rcpp::List::create(
        Rcpp::_["path"] = path_to_r(path),
        Rcpp::_["cleaned"] = !std::filesystem::exists(path));
    END_RCPP
}

static const R_CallMethodDef call_entries[] = {
    {"_panpathAudit_audit", reinterpret_cast<DL_FUNC>(&_panpathAudit_audit), 1},
    {"_panpathAudit_align", reinterpret_cast<DL_FUNC>(&_panpathAudit_align), 3},
    {"_panpathAudit_spool_test", reinterpret_cast<DL_FUNC>(&_panpathAudit_spool_test), 1},
    {nullptr, nullptr, 0}
};

extern "C" void R_init_panpathAudit(DllInfo* dll) {
    R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);
}
