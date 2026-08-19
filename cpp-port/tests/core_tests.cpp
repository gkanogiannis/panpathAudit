#include "panpath/digests.hpp"
#include "panpath/input.hpp"
#include "panpath/graph.hpp"
#include "panpath/memory.hpp"
#include "panpath/statistics.hpp"
#include "panpath/sequence.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>

int main() {
    using panpath::Orientation;
    assert(panpath::complement('R') == 'Y');
    assert(panpath::complement('b') == 'v');
    assert(panpath::segment_position(Orientation::reverse, 0, 4) == 4);
    const auto steps = panpath::walk_steps(">1<two");
    assert(steps.size() == 2 && steps[1].second == Orientation::reverse);

    const std::string lower = "acgt";
    const std::string upper = "ACGT";
    const auto left = panpath::sequence_digest({reinterpret_cast<const std::uint8_t*>(lower.data()), lower.size()});
    const auto right = panpath::sequence_digest({reinterpret_cast<const std::uint8_t*>(upper.data()), upper.size()});
    assert(left.sha256 == right.sha256 && left.blake3 == right.blake3);

    const auto path = std::filesystem::temp_directory_path() / "panpath-cpp-fasta-test.fa";
    { std::ofstream file(path); file << ">x description\nAC\nGT\n"; }
    std::string name, sequence;
    panpath::stream_fasta(path, [&](const auto& n, const auto& s) { name = n; sequence = s; });
    std::filesystem::remove(path);
    assert(name == "x" && sequence == "ACGT");

    const auto graph_path = std::filesystem::temp_directory_path() / "panpath-cpp-graph-test.gfa";
    { std::ofstream file(graph_path); file << "S\t1\tAC\nP\tx\t1+\t*\n"; }
    const auto graph = panpath::read_graph(graph_path, {{"x", 2}}, std::filesystem::temp_directory_path());
    std::filesystem::remove(graph_path);
    assert(graph.errors.empty());
    assert(graph.graph.path_record_count == 1);
    assert(panpath::read_steps(graph.graph, graph.graph.records.at("x").front()).front().segment == "1");
    assert(graph.graph.segment_spool->read(graph.graph.segments.at("1")) == "AC");

    panpath::MemoryBudget budget(10);
    { auto permit = budget.acquire(12); assert(budget.telemetry().peak_tracked_bytes == 12); }
    assert(budget.telemetry().oversized_contigs == 1);
    assert(!budget.acquire_within_limit(11));

    const std::string changed = "ACTT";
    const auto alignment = panpath::align(
        {reinterpret_cast<const std::uint8_t*>(upper.data()), upper.size()},
        {reinterpret_cast<const std::uint8_t*>(changed.data()), changed.size()}, 100);
    assert(alignment.counts && alignment.counts->matched == 3 && alignment.counts->substituted == 1);
    const std::string short_graph = "AGT";
    const auto deletion = panpath::align(
        {reinterpret_cast<const std::uint8_t*>(upper.data()), upper.size()},
        {reinterpret_cast<const std::uint8_t*>(short_graph.data()), short_graph.size()}, 100);
    assert(deletion.counts && deletion.counts->matched == 3 && deletion.counts->source_only == 1);
    const auto limited = panpath::align(
        {reinterpret_cast<const std::uint8_t*>(upper.data()), upper.size()},
        {reinterpret_cast<const std::uint8_t*>(changed.data()), changed.size()}, 1);
    assert(!limited.counts);
}
