#pragma once

#include "panpath/audit.hpp"
#include "panpath/diagnostic.hpp"

#include <optional>
#include <string>
#include <vector>

namespace panpath {

struct Provenance {
    std::vector<std::string> fasta_compressions;
    std::string gfa_version, gfa_compression;
    std::size_t path_records = 0, walk_records = 0;
    bool unspecified_overlaps_as_blunt = false;
};

struct EngineResponse {
    std::optional<AuditResult> result;
    std::optional<Provenance> provenance;
    std::vector<Diagnostic> diagnostics;
};

EngineResponse run_engine(const AuditRequest& request,
                          const ProgressCallback& progress = {});

}
