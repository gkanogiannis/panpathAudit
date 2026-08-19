#include "panpath/audit.hpp"
#include "panpath/input.hpp"
#include "panpath/spool.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>

namespace panpath {
namespace {
template<class T> class Queue {
public:
    bool push(T value) {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return closed_ || values_.size() < 1; });
        if (closed_) return false;
        values_.push_back(std::move(value)); changed_.notify_all(); return true;
    }
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return closed_ || !values_.empty(); });
        if (values_.empty()) return std::nullopt;
        T value = std::move(values_.front()); values_.pop_front(); changed_.notify_all(); return value;
    }
    void close() { std::lock_guard lock(mutex_); closed_ = true; changed_.notify_all(); }
private:
    std::mutex mutex_; std::condition_variable changed_; std::deque<T> values_; bool closed_ = false;
};

struct SourceJob {
    std::size_t source_index;
    std::string identifier;
    std::string sequence;
    MemoryPermit permit;
};

struct AlignmentJob {
    Outcome outcome;
    std::size_t source_index;
    std::shared_ptr<Spool> source_spool;
    RecordRef source_record;
    std::uint64_t source_ambiguous_bases;
};

std::uint64_t ambiguous(std::string_view value) {
    return std::count_if(value.begin(), value.end(), [](unsigned char symbol) { return is_ambiguous(symbol); });
}

RecordType type_of(const Graph& graph, const std::string& name) {
    return graph.records.at(name).front().type;
}

Outcome compare(const Graph& graph, const std::string& name, const std::string& source) {
    const auto& records = graph.records.at(name);
    const auto type = type_of(graph, name);
    Outcome outcome; outcome.identifier = name; outcome.status = Status::identical;
    outcome.record_type = type; outcome.source_length = source.size();
    std::size_t minimum = std::numeric_limits<std::size_t>::max(), maximum = 0;
    bool bounded = true;
    for (const auto& record : records) {
        if (!record.start || !record.end) bounded = false;
        else { minimum = std::min(minimum, *record.start); maximum = std::max(maximum, *record.end); }
    }
    if (bounded) outcome.source_range = {{minimum, maximum}};
    outcome.source_digest = sequence_digest({reinterpret_cast<const std::uint8_t*>(source.data()), source.size()});
    SequenceHasher addressed_hasher, graph_hasher;
    if (!bounded || type == RecordType::path)
        { addressed_hasher.update({reinterpret_cast<const std::uint8_t*>(source.data()), source.size()}); outcome.addressed_source_bases = source.size(); }
    else for (const auto& record : records) {
        addressed_hasher.update({reinterpret_cast<const std::uint8_t*>(source.data() + *record.start), *record.end - *record.start});
        outcome.addressed_source_bases += *record.end - *record.start;
    }
    for (const auto& record : records) {
        const std::size_t start = record.start.value_or(0);
        const auto expected = record.end && record.start ? std::optional(*record.end - *record.start) : std::nullopt;
        std::size_t local = 0;
        for (const auto& step : read_steps(graph, record)) {
            const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
            for (std::size_t index = 0; index < segment.size(); ++index) {
                const auto stored = step.orientation == Orientation::forward ? segment[index] : segment[segment.size() - index - 1];
                const auto base = step.orientation == Orientation::forward ? stored : complement(stored);
                graph_hasher.update({reinterpret_cast<const std::uint8_t*>(&base), 1});
                outcome.graph_ambiguous_bases += is_ambiguous(base);
                const auto source_position = start + local;
                if (!outcome.divergence && ((expected && local >= *expected) || source_position >= source.size() ||
                    std::toupper(static_cast<unsigned char>(source[source_position])) != std::toupper(base))) {
                    outcome.divergence = source_position;
                    outcome.location = Location{step.segment, step.orientation, index + 1,
                        segment_position(step.orientation, index, segment.size())};
                }
                ++local; ++outcome.graph_length;
            }
        }
        if (expected && local != *expected) {
            const auto position = start + std::min(local, *expected);
            if (!outcome.divergence || position < *outcome.divergence) outcome.divergence = position;
            outcome.range_lengths = {{*expected, local}};
            outcome.range_mismatch_position = position;
        }
    }
    if (type == RecordType::path && !outcome.divergence && source.size() != outcome.graph_length)
        outcome.divergence = std::min(source.size(), outcome.graph_length);
    if (outcome.divergence) outcome.status = Status::divergent;
    outcome.addressed_source_digest = addressed_hasher.finish();
    outcome.graph_digest = graph_hasher.finish();
    return outcome;
}

Outcome missing_source(const Graph& graph, const std::string& name) {
    Outcome outcome; outcome.identifier = name; outcome.status = Status::missing_source;
    outcome.record_type = type_of(graph, name);
    SequenceHasher hasher;
    for (const auto& record : graph.records.at(name)) for (const auto& step : read_steps(graph, record)) {
        const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
        for (std::size_t index = 0; index < segment.size(); ++index) {
            const auto stored = step.orientation == Orientation::forward ? segment[index] : segment[segment.size() - index - 1];
            const auto base = step.orientation == Orientation::forward ? stored : complement(stored);
            hasher.update({reinterpret_cast<const std::uint8_t*>(&base), 1}); ++outcome.graph_length;
            outcome.graph_ambiguous_bases += is_ambiguous(base);
        }
    }
    outcome.graph_digest = hasher.finish();
    OutcomeStatistics statistics; statistics.graph_bases = outcome.graph_length;
    statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
    statistics.bases.missing_source = outcome.graph_length;
    statistics.alignment_status = AlignmentStatus::not_applicable;
    outcome.statistics = statistics;
    return outcome;
}

void add_statistics(Outcome& outcome, const Graph& graph, const std::string& source,
                    std::size_t source_index, std::uint64_t max_cells) {
    OutcomeStatistics statistics; statistics.source_index = source_index;
    statistics.source_bases = source.size(); statistics.graph_bases = outcome.graph_length;
    statistics.source_ambiguous_bases = ambiguous(source);
    const auto addressed = outcome.addressed_source_bases;
    statistics.bases.not_embedded = source.size() - addressed;
    statistics.length_delta = static_cast<std::int64_t>(outcome.graph_length) - static_cast<std::int64_t>(addressed);
    if (outcome.status == Status::missing_path) {
        statistics.graph_bases = 0; statistics.length_delta.reset(); statistics.bases.not_embedded = 0;
        statistics.bases.missing_path = source.size(); statistics.alignment_status = AlignmentStatus::not_applicable;
    } else if (outcome.status == Status::identical) {
        statistics.bases.matched = outcome.graph_length; statistics.alignment_status = AlignmentStatus::not_required;
        statistics.edit_distance = 0; statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
        statistics.ambiguous_columns = statistics.graph_ambiguous_bases;
    } else {
        EditCounts total; std::uint64_t cells = 0, graph_ambiguous = 0; bool available = true;
        for (const auto& record : graph.records.at(outcome.identifier)) {
            std::string graph_part;
            for (const auto& step : read_steps(graph, record)) {
                const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
                for (std::size_t i = 0; i < segment.size(); ++i) {
                    const auto stored = step.orientation == Orientation::forward ? segment[i] : segment[segment.size() - i - 1];
                    graph_part.push_back(step.orientation == Orientation::forward ? stored : complement(stored));
                }
            }
            graph_ambiguous += ambiguous(graph_part);
            const auto start = record.start.value_or(0); const auto length = record.end ? *record.end - start : source.size();
            const auto source_part = std::string_view(source).substr(start, length);
            const auto aligned = align({reinterpret_cast<const std::uint8_t*>(source_part.data()), source_part.size()},
                                       {reinterpret_cast<const std::uint8_t*>(graph_part.data()), graph_part.size()}, max_cells - std::min(max_cells, cells));
            if (!aligned.counts) { available = false; break; }
            cells += aligned.cells; total.add(*aligned.counts);
        }
        statistics.graph_ambiguous_bases = graph_ambiguous;
        if (available) {
            statistics.bases.matched = total.matched; statistics.bases.substituted = total.substituted;
            statistics.bases.source_only = total.source_only; statistics.bases.graph_only = total.graph_only;
            statistics.ambiguous_columns = total.ambiguous_columns; statistics.edit_distance = total.edit_distance();
            statistics.alignment_status = AlignmentStatus::complete;
        } else {
            statistics.bases.unaligned_divergent_source = addressed;
            statistics.bases.unaligned_divergent_graph = outcome.graph_length;
            statistics.alignment_status = AlignmentStatus::limit_exceeded;
        }
    }
    outcome.statistics = statistics;
}

void add_unaligned_statistics(Outcome& outcome, std::size_t source_length,
                              std::uint64_t source_ambiguous_bases, std::size_t source_index) {
    OutcomeStatistics statistics; statistics.source_index = source_index;
    statistics.source_bases = source_length; statistics.graph_bases = outcome.graph_length;
    statistics.source_ambiguous_bases = source_ambiguous_bases;
    statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
    statistics.bases.not_embedded = source_length - outcome.addressed_source_bases;
    statistics.bases.unaligned_divergent_source = outcome.addressed_source_bases;
    statistics.bases.unaligned_divergent_graph = outcome.graph_length;
    statistics.length_delta = static_cast<std::int64_t>(outcome.graph_length) - static_cast<std::int64_t>(outcome.addressed_source_bases);
    statistics.alignment_status = AlignmentStatus::memory_limit_exceeded;
    outcome.statistics = statistics;
}
}

AuditResult audit_basic(const Config& config, const Graph& graph) {
    std::vector<Outcome> outcomes;
    std::set<std::string> sources;
    MemoryBudget budget(config.memory_mib * 1024 * 1024);
    Queue<SourceJob> queue;
    Queue<AlignmentJob> alignment_queue;
    std::mutex result_mutex;
    std::exception_ptr failure;
    std::thread aligner([&] {
        try {
            while (auto job = alignment_queue.pop()) {
                const auto tracked = job->source_record.length + job->outcome.graph_length;
                auto permit = budget.acquire_within_limit(tracked);
                if (permit) {
                    auto source = job->source_spool->read(job->source_record);
                    add_statistics(job->outcome, graph, source, job->source_index, config.alignment_max_cells);
                } else add_unaligned_statistics(job->outcome, job->source_record.length,
                    job->source_ambiguous_bases, job->source_index);
                std::lock_guard lock(result_mutex); outcomes.push_back(std::move(job->outcome));
            }
        } catch (...) {
            std::lock_guard lock(result_mutex); if (!failure) failure = std::current_exception();
            queue.close(); alignment_queue.close();
        }
    });
    std::vector<std::thread> workers;
    for (std::size_t i = 0; i < config.threads; ++i) workers.emplace_back([&] {
        try {
            while (auto job = queue.pop()) {
                Outcome outcome;
                if (!graph.path_names.contains(job->identifier)) {
                    outcome.identifier = job->identifier; outcome.status = Status::missing_path;
                    outcome.source_length = job->sequence.size();
                } else outcome = compare(graph, job->identifier, job->sequence);
                if (outcome.status == Status::missing_path)
                    outcome.source_digest = sequence_digest({reinterpret_cast<const std::uint8_t*>(job->sequence.data()), job->sequence.size()});
                if (config.comprehensive && outcome.status == Status::divergent) {
                    auto spool = std::make_shared<Spool>(config.temp_dir);
                    const auto record = spool->append({job->sequence.data(), job->sequence.size()});
                    const auto source_ambiguous_bases = ambiguous(job->sequence);
                    job->sequence.clear(); job->sequence.shrink_to_fit(); job->permit = MemoryPermit{};
                    if (!alignment_queue.push({std::move(outcome), job->source_index, std::move(spool), record, source_ambiguous_bases})) break;
                } else {
                    if (config.comprehensive) add_statistics(outcome, graph, job->sequence, job->source_index, config.alignment_max_cells);
                    std::lock_guard lock(result_mutex); outcomes.push_back(std::move(outcome));
                }
            }
        } catch (...) {
            std::lock_guard lock(result_mutex);
            if (!failure) failure = std::current_exception();
            queue.close();
            alignment_queue.close();
        }
    });
    try {
        for (std::size_t source_index = 0; source_index < config.sources.size(); ++source_index) {
            const auto& spec = config.sources[source_index];
            stream_fasta(spec.path, [&](const std::string& raw, const std::string& sequence) {
                const auto name = spec.identifier(raw); sources.insert(name);
                auto permit = budget.acquire(sequence.size());
                if (!queue.push({source_index, name, sequence, std::move(permit)})) throw std::runtime_error("audit worker stopped");
            });
        }
    } catch (...) {
        std::lock_guard lock(result_mutex);
        if (!failure) failure = std::current_exception();
    }
    queue.close();
    for (auto& worker : workers) worker.join();
    alignment_queue.close();
    aligner.join();
    if (failure) std::rethrow_exception(failure);
    for (const auto& name : graph.path_names) if (!sources.contains(name))
        { auto outcome = missing_source(graph, name); if (!config.comprehensive) outcome.statistics.reset(); outcomes.push_back(std::move(outcome)); }
    std::sort(outcomes.begin(), outcomes.end(), [](const auto& a, const auto& b) { return a.identifier < b.identifier; });
    return {std::move(outcomes), budget.telemetry()};
}
}
