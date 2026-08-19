#pragma once

#include "panpath/audit.hpp"
#include "panpath/cli.hpp"
#include "panpath/graph.hpp"
#include "panpath/input.hpp"

#include <string>

namespace panpath {

std::string render_json(const Config& config, const Graph& graph,
                        const SourceIndex& sources, const AuditResult& audit);
std::string render_tsv(const Graph& graph, const AuditResult& audit);
std::string render_invalid_json(const std::vector<std::string>& source_errors,
                                const std::vector<std::string>& graph_errors,
                                const std::vector<std::string>& operational_errors,
                                bool comprehensive);
std::string render_invalid_tsv(const std::vector<std::string>& source_errors,
                               const std::vector<std::string>& graph_errors,
                               const std::vector<std::string>& operational_errors);

}
