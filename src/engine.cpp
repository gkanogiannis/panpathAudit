#include "panpath/engine.hpp"
#include "panpath/graph.hpp"
#include "panpath/input.hpp"

#include <exception>
#include <new>

namespace panpath {

EngineResponse run_engine(const AuditRequest& request, const ProgressCallback& progress) {
    EngineResponse response;
    auto sources = index_sources(request.sources, progress);
    response.diagnostics = std::move(sources.diagnostics);

    std::optional<GraphReadResult> graph;
    try {
        graph = read_graph(request.gfa_path, sources.lengths,
            request.resources.temp_dir, progress);
        response.diagnostics.insert(response.diagnostics.end(),
            std::make_move_iterator(graph->diagnostics.begin()),
            std::make_move_iterator(graph->diagnostics.end()));
    } catch (const std::runtime_error& error) {
        const std::string message(error.what());
        const bool temporary = message.starts_with("TEMP_STORE");
        response.diagnostics.push_back({
            temporary ? DiagnosticCategory::operational : DiagnosticCategory::gfa,
            temporary ? "TEMP_STORE" : "GFA_READ", message,
            temporary ? std::optional<std::filesystem::path>{} : request.gfa_path});
    } catch (const std::bad_alloc&) {
        response.diagnostics.push_back({DiagnosticCategory::operational, "OUT_OF_MEMORY",
            "memory allocation failed"});
    }
    if (!response.diagnostics.empty() || !graph) return response;

    response.provenance = Provenance{sources.compressions, graph->graph.version,
        graph->graph.compression, graph->graph.path_record_count,
        graph->graph.walk_record_count, graph->graph.unspecified_overlaps_as_blunt};
    try {
        Cancellation cancellation;
        response.result = audit(request, graph->graph, cancellation, progress);
    } catch (const std::runtime_error& error) {
        response.diagnostics.push_back({DiagnosticCategory::operational,
            "AUDIT_FAILED", error.what()});
    } catch (const std::bad_alloc&) {
        response.diagnostics.push_back({DiagnosticCategory::operational,
            "OUT_OF_MEMORY", "memory allocation failed"});
    }
    return response;
}

}
