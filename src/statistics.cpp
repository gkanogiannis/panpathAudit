#include "panpath/statistics.hpp"
#include "panpath/sequence.hpp"

#include <cctype>
#include <vector>

namespace panpath {

void EditCounts::add(const EditCounts& other) {
    matched += other.matched;
    substituted += other.substituted;
    source_only += other.source_only;
    graph_only += other.graph_only;
    ambiguous_columns += other.ambiguous_columns;
}

namespace {

struct Cell { std::size_t source; EditCounts counts; };
enum class Operation { substitute, source_only, graph_only };

Cell extend(std::span<const std::uint8_t> source, std::span<const std::uint8_t> graph,
            std::size_t source_position, std::size_t graph_position, EditCounts counts) {
    while (source_position < source.size() && graph_position < graph.size() &&
           std::toupper(source[source_position]) == std::toupper(graph[graph_position])) {
        ++counts.matched;
        counts.ambiguous_columns += is_ambiguous(source[source_position]) ||
            is_ambiguous(graph[graph_position]);
        ++source_position;
        ++graph_position;
    }
    return {source_position, counts};
}

std::optional<Cell> prior(const std::vector<std::optional<Cell>>& layer,
                          std::size_t score, std::int64_t diagonal) {
    if (diagonal < -static_cast<std::int64_t>(score) ||
        diagonal > static_cast<std::int64_t>(score)) return std::nullopt;
    return layer[static_cast<std::size_t>(diagonal + score)];
}

void consider(std::optional<Cell>& best, std::optional<Cell> previous,
              std::span<const std::uint8_t> source,
              std::span<const std::uint8_t> graph,
              std::int64_t diagonal, Operation operation) {
    if (!previous) return;
    auto cell = *previous;
    const auto old_diagonal = operation == Operation::substitute ? diagonal :
        operation == Operation::source_only ? diagonal - 1 : diagonal + 1;
    const auto graph_position = static_cast<std::int64_t>(cell.source) - old_diagonal;
    const auto next_source = cell.source + (operation == Operation::graph_only ? 0 : 1);
    const auto next_graph_signed = graph_position + (operation == Operation::source_only ? 0 : 1);
    if (next_graph_signed < 0) return;
    const auto next_graph = static_cast<std::size_t>(next_graph_signed);
    if (next_source > source.size() || next_graph > graph.size()) return;
    if (operation == Operation::substitute) {
        if (!next_source || !next_graph ||
            std::toupper(source[next_source - 1]) == std::toupper(graph[next_graph - 1])) return;
        ++cell.counts.substituted;
        cell.counts.ambiguous_columns += is_ambiguous(source[next_source - 1]) ||
            is_ambiguous(graph[next_graph - 1]);
    } else if (operation == Operation::source_only) {
        if (!next_source) return;
        ++cell.counts.source_only;
        cell.counts.ambiguous_columns += is_ambiguous(source[next_source - 1]);
    } else {
        if (!next_graph) return;
        ++cell.counts.graph_only;
        cell.counts.ambiguous_columns += is_ambiguous(graph[next_graph - 1]);
    }
    const auto candidate = extend(source, graph, next_source, next_graph, cell.counts);
    if (!best || candidate.source > best->source) best = candidate;
}

}

Alignment align(std::span<const std::uint8_t> source,
                std::span<const std::uint8_t> graph,
                std::uint64_t max_cells,
                const Cancellation* cancellation) {
    if (!max_cells) return {};
    std::uint64_t cells = 1;
    const auto initial = extend(source, graph, 0, 0, {});
    if (initial.source == source.size() && source.size() == graph.size())
        return {initial.counts, cells};
    std::vector<std::optional<Cell>> previous{initial};
    const auto target = static_cast<std::int64_t>(source.size()) -
        static_cast<std::int64_t>(graph.size());
    for (std::size_t score = 1; score <= source.size() + graph.size(); ++score) {
        if (cancellation) cancellation->check();
        std::vector<std::optional<Cell>> current(score * 2 + 1);
        for (auto diagonal = -static_cast<std::int64_t>(score);
             diagonal <= static_cast<std::int64_t>(score); ++diagonal) {
            if (cancellation && (cells & 4095U) == 0) cancellation->check();
            if (++cells > max_cells) return {};
            std::optional<Cell> best;
            consider(best, prior(previous, score - 1, diagonal), source, graph,
                diagonal, Operation::substitute);
            consider(best, prior(previous, score - 1, diagonal - 1), source, graph,
                diagonal, Operation::source_only);
            consider(best, prior(previous, score - 1, diagonal + 1), source, graph,
                diagonal, Operation::graph_only);
            if (!best) continue;
            current[static_cast<std::size_t>(diagonal + score)] = best;
            if (diagonal == target && best->source == source.size() &&
                static_cast<std::int64_t>(best->source) - diagonal ==
                    static_cast<std::int64_t>(graph.size()))
                return {best->counts, cells};
        }
        previous = std::move(current);
    }
    return {};
}

}
