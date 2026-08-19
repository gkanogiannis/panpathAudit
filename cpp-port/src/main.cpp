#include "panpath/audit.hpp"
#include "panpath/cli.hpp"
#include "panpath/graph.hpp"
#include "panpath/input.hpp"
#include "panpath/report.hpp"

#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
int failure_exit(const std::vector<std::string>& source, const std::vector<std::string>& graph) {
    if (!source.empty()) return 4;
    const char* ambiguous[] = {"DUPLICATE_PATH", "DUPLICATE_EMBEDDED_PATH", "AMBIGUOUS_W_RANGES", "W_RANGE_OVERLAP"};
    for (const auto& error : graph) {
        bool found = false;
        for (const auto* code : ambiguous) found |= error.starts_with(code);
        if (!found) return 3;
    }
    return 2;
}
}

int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "-h" || argument == "--help") {
            std::cout << panpath::help_text() << '\n';
            return 0;
        }
        if (argument == "-V" || argument == "--version") {
            std::cout << "panpath-audit-cpp " << PANPATH_VERSION << '\n';
            return 0;
        }
        arguments.emplace_back(argument);
    }
    bool json_requested = false, tsv_requested = false, comprehensive_requested = false;
    for (std::size_t index = 0; index + 1 < arguments.size(); ++index) {
        json_requested |= arguments[index] == "--format" && arguments[index + 1] == "json";
        tsv_requested |= arguments[index] == "--format" && arguments[index + 1] == "tsv";
        comprehensive_requested |= arguments[index] == "--stats" && arguments[index + 1] == "comprehensive";
    }
    try {
        const auto config = panpath::parse_arguments(arguments);
        auto sources = panpath::index_sources(config.sources);
        panpath::GraphReadResult graph;
        try { graph = panpath::read_graph(config.gfa_path, sources.lengths, config.temp_dir); }
        catch (const std::exception& error) { graph.errors.push_back(error.what()); }
        if (!sources.errors.empty() || !graph.errors.empty()) {
            if (json_requested) std::cout << panpath::render_invalid_json(sources.errors, graph.errors, {}, comprehensive_requested) << '\n';
            else if (tsv_requested) std::cout << panpath::render_invalid_tsv(sources.errors, graph.errors, {});
            else {
                for (const auto& error : sources.errors) std::cerr << error << '\n';
                for (const auto& error : graph.errors) std::cerr << error << '\n';
                std::cerr << "Try 'panpath-audit --help' for usage.\n";
            }
            return failure_exit(sources.errors, graph.errors);
        }
        const auto audited = panpath::audit_basic(config, graph.graph);
        const auto& outcomes = audited.outcomes;
        if (config.format == panpath::Format::json) {
            std::cout << panpath::render_json(config, graph.graph, sources, audited) << '\n';
            std::size_t divergent = 0, incomplete = 0;
            for (const auto& outcome : outcomes) {
                divergent += outcome.status == panpath::Status::divergent;
                incomplete += outcome.status == panpath::Status::missing_path || outcome.status == panpath::Status::missing_source;
            }
            return incomplete ? 2 : divergent ? 1 : 0;
        }
        if (config.format == panpath::Format::tsv) {
            std::cout << panpath::render_tsv(graph.graph, audited);
            std::size_t divergent = 0, incomplete = 0;
            for (const auto& outcome : outcomes) {
                divergent += outcome.status == panpath::Status::divergent;
                incomplete += outcome.status == panpath::Status::missing_path || outcome.status == panpath::Status::missing_source;
            }
            return incomplete ? 2 : divergent ? 1 : 0;
        }
        std::cout << "PROVENANCE topology_validated=false unspecified_overlaps_as_blunt="
                  << (graph.graph.unspecified_overlaps_as_blunt ? "true" : "false") << '\n';
        std::cout << "MEMORY limit_mib=" << config.memory_mib << " peak_tracked_bytes=" << audited.memory.peak_tracked_bytes
                  << " oversized_contigs=" << audited.memory.oversized_contigs << '\n';
        if (config.comprehensive) {
            panpath::BaseStatistics bases; std::uint64_t source_bases = 0, graph_bases = 0, distance = 0;
            std::uint64_t aligned = 0, unavailable = 0;
            for (const auto& outcome : outcomes) if (outcome.statistics) {
                const auto& s = *outcome.statistics; source_bases += s.source_bases; graph_bases += s.graph_bases;
                bases.matched += s.bases.matched; bases.substituted += s.bases.substituted;
                bases.source_only += s.bases.source_only; bases.graph_only += s.bases.graph_only;
                bases.not_embedded += s.bases.not_embedded; bases.missing_path += s.bases.missing_path;
                bases.missing_source += s.bases.missing_source;
                bases.unaligned_divergent_source += s.bases.unaligned_divergent_source;
                bases.unaligned_divergent_graph += s.bases.unaligned_divergent_graph;
                if (s.edit_distance) { distance += *s.edit_distance; aligned += s.bases.matched + s.bases.substituted + s.bases.source_only + s.bases.graph_only; }
                unavailable += s.alignment_status == panpath::AlignmentStatus::limit_exceeded || s.alignment_status == panpath::AlignmentStatus::memory_limit_exceeded;
            }
            std::cout << "SOURCE_BASES total=" << source_bases << " matched=" << bases.matched << " substituted=" << bases.substituted
                      << " source_only=" << bases.source_only << " not_embedded=" << bases.not_embedded << " missing_path=" << bases.missing_path
                      << " unaligned_divergent=" << bases.unaligned_divergent_source << '\n';
            std::cout << "GRAPH_BASES total=" << graph_bases << " matched=" << bases.matched << " substituted=" << bases.substituted
                      << " graph_only=" << bases.graph_only << " missing_source=" << bases.missing_source
                      << " unaligned_divergent=" << bases.unaligned_divergent_graph << '\n';
            const auto percent = [](std::uint64_t numerator, std::uint64_t denominator) {
                if (!denominator) return std::string("null");
                std::ostringstream value; value << std::fixed << std::setprecision(6)
                    << std::round(static_cast<double>(numerator) * 100000000.0 / static_cast<double>(denominator)) / 1000000.0;
                auto text = value.str(); while (text.size() > 2 && text.back() == '0' && text[text.size() - 2] != '.') text.pop_back();
                return text;
            };
            std::cout << "ALIGNMENT identity_percent=" << percent(bases.matched, aligned);
            std::cout << " edit_distance=" << distance << " unavailable=" << unavailable << '\n';
            for (std::size_t index = 0; index < config.sources.size(); ++index) {
                std::uint64_t sequences = 0, total = 0, matched = 0;
                for (const auto& outcome : outcomes) if (outcome.statistics && outcome.statistics->source_index == index) {
                    ++sequences; total += outcome.statistics->source_bases; matched += outcome.statistics->bases.matched;
                }
                std::cout << "SOURCE index=" << index << " sequences=" << sequences << " bases=" << total
                          << " exact_recovery_percent=" << percent(matched, total) << '\n';
            }
        }
        std::size_t identical = 0, divergent = 0, missing_path = 0, missing_source = 0;
        for (const auto& outcome : outcomes) {
            identical += outcome.status == panpath::Status::identical;
            divergent += outcome.status == panpath::Status::divergent;
            missing_path += outcome.status == panpath::Status::missing_path;
            missing_source += outcome.status == panpath::Status::missing_source;
        }
        if (identical) std::cout << "IDENTICAL " << identical << '\n';
        if (divergent) std::cout << "DIVERGENT " << divergent << '\n';
        if (missing_path) std::cout << "MISSING_PATH " << missing_path << '\n';
        if (missing_source) std::cout << "MISSING_SOURCE " << missing_source << '\n';
        for (const auto& outcome : outcomes) if (outcome.status == panpath::Status::divergent) {
            const auto index = *outcome.divergence;
            std::cout << outcome.identifier << " source_position=" << index + 1 << " path_position=" << index + 1
                      << " source_length=" << outcome.source_length << " path_length=" << outcome.graph_length;
            if (outcome.location) std::cout << " segment=" << outcome.location->segment
                << (outcome.location->orientation == panpath::Orientation::forward ? '+' : '-')
                << " traversal_position=" << outcome.location->traversal_position
                << " segment_position=" << outcome.location->segment_position;
            else std::cout << " graph=<END>";
            std::cout << '\n';
        }
        for (const auto& outcome : outcomes) if (outcome.status == panpath::Status::missing_path)
            std::cout << outcome.identifier << " MISSING_PATH\n";
        for (const auto& outcome : outcomes) if (outcome.status == panpath::Status::missing_source)
            std::cout << outcome.identifier << " MISSING_SOURCE\n";
        if (config.comprehensive) for (const auto& outcome : outcomes)
            if (outcome.status != panpath::Status::identical && outcome.statistics) {
                const auto& s = *outcome.statistics;
                const char* alignment = s.alignment_status == panpath::AlignmentStatus::complete ? "complete" :
                    s.alignment_status == panpath::AlignmentStatus::limit_exceeded ? "limit_exceeded" :
                    s.alignment_status == panpath::AlignmentStatus::memory_limit_exceeded ? "memory_limit_exceeded" :
                    s.alignment_status == panpath::AlignmentStatus::not_required ? "not_required" : "not_applicable";
                std::cout << outcome.identifier << " BASES source=" << s.source_bases << " graph=" << s.graph_bases
                          << " matched=" << s.bases.matched << " substituted=" << s.bases.substituted
                          << " source_only=" << s.bases.source_only << " graph_only=" << s.bases.graph_only
                          << " alignment=" << alignment << '\n';
            }
        if (missing_path || missing_source) return 2;
        if (divergent) return 1;
        return 0;
    } catch (const std::exception& error) {
        const std::vector<std::string> errors{error.what()};
        if (json_requested) std::cout << panpath::render_invalid_json({}, {}, errors, comprehensive_requested) << '\n';
        else if (tsv_requested) std::cout << panpath::render_invalid_tsv({}, {}, errors);
        else std::cerr << error.what() << "\nTry 'panpath-audit --help' for usage.\n";
    }
    return 4;
}
