#include "panpath/statistics.hpp"
#include "panpath/sequence.hpp"

#include <algorithm>
#include <vector>

namespace panpath {
void EditCounts::add(const EditCounts& o) { matched += o.matched; substituted += o.substituted; source_only += o.source_only; graph_only += o.graph_only; ambiguous_columns += o.ambiguous_columns; }
namespace {
struct Cell { std::size_t source; EditCounts counts; };
enum class Operation { substitute, source_only, graph_only };

Cell extend(std::span<const std::uint8_t> source, std::span<const std::uint8_t> graph,
            std::size_t s, std::size_t g, EditCounts counts) {
    while (s < source.size() && g < graph.size() && std::toupper(source[s]) == std::toupper(graph[g])) {
        ++counts.matched; counts.ambiguous_columns += is_ambiguous(source[s]) || is_ambiguous(graph[g]); ++s; ++g;
    }
    return {s, counts};
}

std::optional<Cell> prior(const std::vector<std::optional<Cell>>& layer, std::size_t score, std::int64_t k) {
    if (k < -static_cast<std::int64_t>(score) || k > static_cast<std::int64_t>(score)) return std::nullopt;
    return layer[static_cast<std::size_t>(k + score)];
}

void consider(std::optional<Cell>& best, std::optional<Cell> previous,
              std::span<const std::uint8_t> source, std::span<const std::uint8_t> graph,
              std::int64_t k, Operation operation) {
    if (!previous) return;
    auto cell = *previous;
    const auto old_k = operation == Operation::substitute ? k : operation == Operation::source_only ? k - 1 : k + 1;
    const auto graph_position = static_cast<std::int64_t>(cell.source) - old_k;
    const auto next_source = cell.source + (operation == Operation::graph_only ? 0 : 1);
    const auto next_graph_signed = graph_position + (operation == Operation::source_only ? 0 : 1);
    if (next_graph_signed < 0) return;
    const auto next_graph = static_cast<std::size_t>(next_graph_signed);
    if (next_source > source.size() || next_graph > graph.size()) return;
    if (operation == Operation::substitute) {
        if (!next_source || !next_graph || std::toupper(source[next_source - 1]) == std::toupper(graph[next_graph - 1])) return;
        ++cell.counts.substituted; cell.counts.ambiguous_columns += is_ambiguous(source[next_source - 1]) || is_ambiguous(graph[next_graph - 1]);
    } else if (operation == Operation::source_only) {
        if (!next_source) return; ++cell.counts.source_only; cell.counts.ambiguous_columns += is_ambiguous(source[next_source - 1]);
    } else {
        if (!next_graph) return; ++cell.counts.graph_only; cell.counts.ambiguous_columns += is_ambiguous(graph[next_graph - 1]);
    }
    const auto candidate = extend(source, graph, next_source, next_graph, cell.counts);
    if (!best || candidate.source > best->source) best = candidate;
}
}

Alignment align(std::span<const std::uint8_t> source, std::span<const std::uint8_t> graph, std::uint64_t max_cells) {
    if (!max_cells) return {};
    std::uint64_t cells = 1;
    const auto initial = extend(source, graph, 0, 0, {});
    if (initial.source == source.size() && source.size() == graph.size()) return {initial.counts, cells};
    std::vector<std::optional<Cell>> previous{initial};
    const auto target = static_cast<std::int64_t>(source.size()) - static_cast<std::int64_t>(graph.size());
    for (std::size_t score = 1; score <= source.size() + graph.size(); ++score) {
        std::vector<std::optional<Cell>> current(score * 2 + 1);
        for (auto k = -static_cast<std::int64_t>(score); k <= static_cast<std::int64_t>(score); ++k) {
            if (++cells > max_cells) return {};
            std::optional<Cell> best;
            consider(best, prior(previous, score - 1, k), source, graph, k, Operation::substitute);
            consider(best, prior(previous, score - 1, k - 1), source, graph, k, Operation::source_only);
            consider(best, prior(previous, score - 1, k + 1), source, graph, k, Operation::graph_only);
            if (!best) continue;
            current[static_cast<std::size_t>(k + score)] = best;
            if (k == target && best->source == source.size() && static_cast<std::int64_t>(best->source) - k == static_cast<std::int64_t>(graph.size()))
                return {best->counts, cells};
        }
        previous = std::move(current);
    }
    return {};
}
}
