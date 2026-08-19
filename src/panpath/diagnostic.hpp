#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace panpath {

enum class DiagnosticCategory { source, gfa, correspondence, operational, internal };

struct Diagnostic {
    Diagnostic() = default;
    Diagnostic(DiagnosticCategory category_value, std::string code_value,
               std::string message_value,
               std::optional<std::filesystem::path> path_value = {})
        : category(category_value), code(std::move(code_value)),
          message(std::move(message_value)), path(std::move(path_value)) {}

    DiagnosticCategory category = DiagnosticCategory::internal;
    std::string code;
    std::string message;
    std::optional<std::filesystem::path> path;
    std::optional<std::size_t> line;
    std::optional<std::string> identifier;
    std::optional<std::size_t> position;
    std::optional<std::string> symbol;
    std::map<std::string, std::string> details;
};

const char* category_name(DiagnosticCategory category);

}
