#pragma once

#include "panpath/cancellation.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace panpath {

struct EditCounts {
    std::uint64_t matched = 0, substituted = 0, source_only = 0, graph_only = 0;
    std::uint64_t ambiguous_columns = 0;
    std::uint64_t edit_distance() const { return substituted + source_only + graph_only; }
    void add(const EditCounts& other);
};

struct Alignment { std::optional<EditCounts> counts; std::uint64_t cells = 0; };
Alignment align(std::span<const std::uint8_t> source,
                std::span<const std::uint8_t> graph,
                std::uint64_t max_cells,
                const Cancellation* cancellation = nullptr);

}
