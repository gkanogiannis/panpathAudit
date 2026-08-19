#include "panpath/audit.hpp"
#include "panpath/statistics.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace panpath {
namespace {

template<class T>
class Queue {
public:
    bool push(T value, const Cancellation& cancellation,
              const ProgressCallback& progress = {}) {
        std::unique_lock lock(mutex_);
        while (!closed_ && values_.size() >= 1 && !cancellation.cancelled()) {
            lock.unlock();
            if (progress) progress();
            lock.lock();
            changed_.wait_for(lock, std::chrono::milliseconds(25));
        }
        if (closed_ || cancellation.cancelled()) return false;
        values_.push_back(std::move(value));
        changed_.notify_all();
        return true;
    }

    std::optional<T> pop(const Cancellation& cancellation) {
        std::unique_lock lock(mutex_);
        while (!closed_ && values_.empty() && !cancellation.cancelled())
            changed_.wait_for(lock, std::chrono::milliseconds(25));
        if (values_.empty()) return std::nullopt;
        T value = std::move(values_.front());
        values_.pop_front();
        changed_.notify_all();
        return value;
    }

    void close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        changed_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<T> values_;
    bool closed_ = false;
};

struct SourceJob {
    std::size_t source_index;
    std::string identifier, sequence;
    MemoryPermit permit;
};

struct AlignmentJob {
    Outcome outcome;
    std::shared_ptr<Spool> source_spool;
    RecordRef source_record;
    std::uint64_t source_ambiguous_bases;
};

std::uint64_t ambiguous(std::string_view value) {
    return std::count_if(value.begin(), value.end(),
        [](unsigned char symbol) { return is_ambiguous(symbol); });
}

RecordType type_of(const Graph& graph, const std::string& name) {
    return graph.records.at(name).front().type;
}

Outcome compare(const Graph& graph, const std::string& name,
                const std::string& source, const Cancellation& cancellation) {
    const auto& records = graph.records.at(name);
    const auto type = type_of(graph, name);
    Outcome outcome;
    outcome.identifier = name;
    outcome.status = Status::identical;
    outcome.record_type = type;
    outcome.source_length = source.size();
    std::size_t minimum = std::numeric_limits<std::size_t>::max(), maximum = 0;
    bool bounded = true;
    for (const auto& record : records) {
        if (!record.start || !record.end) bounded = false;
        else {
            minimum = std::min(minimum, *record.start);
            maximum = std::max(maximum, *record.end);
        }
    }
    if (bounded) outcome.source_range = {{minimum, maximum}};
    outcome.source_digest = sequence_digest({
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size()});
    SequenceHasher addressed_hasher, graph_hasher;
    if (!bounded || type == RecordType::path) {
        addressed_hasher.update({reinterpret_cast<const std::uint8_t*>(source.data()), source.size()});
        outcome.addressed_source_bases = source.size();
    } else {
        for (const auto& record : records) {
            addressed_hasher.update({
                reinterpret_cast<const std::uint8_t*>(source.data() + *record.start),
                *record.end - *record.start});
            outcome.addressed_source_bases += *record.end - *record.start;
        }
    }
    for (const auto& record : records) {
        cancellation.check();
        const std::size_t start = record.start.value_or(0);
        const auto expected = record.end && record.start
            ? std::optional(*record.end - *record.start) : std::nullopt;
        std::size_t local = 0;
        for (const auto& step : read_steps(graph, record)) {
            cancellation.check();
            const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
            for (std::size_t index = 0; index < segment.size(); ++index) {
                if ((index & 65535U) == 0) cancellation.check();
                const auto stored = step.orientation == Orientation::forward
                    ? segment[index] : segment[segment.size() - index - 1];
                const auto base = step.orientation == Orientation::forward
                    ? static_cast<unsigned char>(stored) : complement(stored);
                graph_hasher.update({reinterpret_cast<const std::uint8_t*>(&base), 1});
                outcome.graph_ambiguous_bases += is_ambiguous(base);
                const auto source_position = start + local;
                if (!outcome.divergence &&
                    ((expected && local >= *expected) || source_position >= source.size() ||
                     std::toupper(static_cast<unsigned char>(source[source_position])) !=
                         std::toupper(base))) {
                    outcome.divergence = source_position;
                    outcome.location = Location{step.segment, step.orientation, index + 1,
                        segment_position(step.orientation, index, segment.size())};
                }
                ++local;
                ++outcome.graph_length;
            }
        }
        if (expected && local != *expected) {
            const auto position = start + std::min(local, *expected);
            if (!outcome.divergence || position < *outcome.divergence)
                outcome.divergence = position;
            outcome.range_lengths = {{*expected, local}};
            outcome.range_mismatch_position = position;
        }
    }
    if (type == RecordType::path && !outcome.divergence &&
        source.size() != outcome.graph_length)
        outcome.divergence = std::min(source.size(), outcome.graph_length);
    if (outcome.divergence) outcome.status = Status::divergent;
    outcome.addressed_source_digest = addressed_hasher.finish();
    outcome.graph_digest = graph_hasher.finish();
    return outcome;
}

Outcome missing_source(const Graph& graph, const std::string& name,
                       const Cancellation& cancellation) {
    Outcome outcome;
    outcome.identifier = name;
    outcome.status = Status::missing_source;
    outcome.record_type = type_of(graph, name);
    SequenceHasher hasher;
    for (const auto& record : graph.records.at(name)) {
        cancellation.check();
        for (const auto& step : read_steps(graph, record)) {
            const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
            for (std::size_t index = 0; index < segment.size(); ++index) {
                if ((index & 65535U) == 0) cancellation.check();
                const auto stored = step.orientation == Orientation::forward
                    ? segment[index] : segment[segment.size() - index - 1];
                const auto base = step.orientation == Orientation::forward
                    ? static_cast<unsigned char>(stored) : complement(stored);
                hasher.update({reinterpret_cast<const std::uint8_t*>(&base), 1});
                ++outcome.graph_length;
                outcome.graph_ambiguous_bases += is_ambiguous(base);
            }
        }
    }
    outcome.graph_digest = hasher.finish();
    OutcomeStatistics statistics;
    statistics.graph_bases = outcome.graph_length;
    statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
    statistics.bases.missing_source = outcome.graph_length;
    outcome.statistics = statistics;
    return outcome;
}

void add_statistics(Outcome& outcome, const Graph& graph, const std::string& source,
                    std::uint64_t max_cells, const Cancellation& cancellation) {
    OutcomeStatistics statistics;
    statistics.source_bases = source.size();
    statistics.graph_bases = outcome.graph_length;
    statistics.source_ambiguous_bases = ambiguous(source);
    const auto addressed = outcome.addressed_source_bases;
    statistics.bases.not_embedded = source.size() - addressed;
    statistics.length_delta = static_cast<std::int64_t>(outcome.graph_length) -
        static_cast<std::int64_t>(addressed);
    if (outcome.status == Status::missing_path) {
        statistics.graph_bases = 0;
        statistics.length_delta.reset();
        statistics.bases.not_embedded = 0;
        statistics.bases.missing_path = source.size();
    } else if (outcome.status == Status::identical) {
        statistics.bases.matched = outcome.graph_length;
        statistics.alignment_status = AlignmentStatus::not_required;
        statistics.edit_distance = 0;
        statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
        statistics.ambiguous_columns = statistics.graph_ambiguous_bases;
    } else {
        EditCounts total;
        std::uint64_t cells = 0, graph_ambiguous = 0;
        bool available = true;
        for (const auto& record : graph.records.at(outcome.identifier)) {
            cancellation.check();
            std::string graph_part;
            for (const auto& step : read_steps(graph, record)) {
                const auto segment = graph.segment_spool->read(graph.segments.at(step.segment));
                for (std::size_t index = 0; index < segment.size(); ++index) {
                    if ((index & 65535U) == 0) cancellation.check();
                    const auto stored = step.orientation == Orientation::forward
                        ? segment[index] : segment[segment.size() - index - 1];
                    graph_part.push_back(step.orientation == Orientation::forward
                        ? stored : complement(stored));
                }
            }
            graph_ambiguous += ambiguous(graph_part);
            const auto start = record.start.value_or(0);
            const auto length = record.end ? *record.end - start : source.size();
            const auto source_part = std::string_view(source).substr(start, length);
            const auto remaining = max_cells - std::min(max_cells, cells);
            const auto aligned = align(
                {reinterpret_cast<const std::uint8_t*>(source_part.data()), source_part.size()},
                {reinterpret_cast<const std::uint8_t*>(graph_part.data()), graph_part.size()},
                remaining, &cancellation);
            if (!aligned.counts) { available = false; break; }
            cells += aligned.cells;
            total.add(*aligned.counts);
        }
        statistics.graph_ambiguous_bases = graph_ambiguous;
        if (available) {
            statistics.bases.matched = total.matched;
            statistics.bases.substituted = total.substituted;
            statistics.bases.source_only = total.source_only;
            statistics.bases.graph_only = total.graph_only;
            statistics.ambiguous_columns = total.ambiguous_columns;
            statistics.edit_distance = total.edit_distance();
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
                              std::uint64_t source_ambiguous_bases) {
    OutcomeStatistics statistics;
    statistics.source_bases = source_length;
    statistics.graph_bases = outcome.graph_length;
    statistics.source_ambiguous_bases = source_ambiguous_bases;
    statistics.graph_ambiguous_bases = outcome.graph_ambiguous_bases;
    statistics.bases.not_embedded = source_length - outcome.addressed_source_bases;
    statistics.bases.unaligned_divergent_source = outcome.addressed_source_bases;
    statistics.bases.unaligned_divergent_graph = outcome.graph_length;
    statistics.length_delta = static_cast<std::int64_t>(outcome.graph_length) -
        static_cast<std::int64_t>(outcome.addressed_source_bases);
    statistics.alignment_status = AlignmentStatus::memory_limit_exceeded;
    outcome.statistics = statistics;
}

}

void BaseStatistics::add(const BaseStatistics& other) {
    matched += other.matched;
    substituted += other.substituted;
    source_only += other.source_only;
    graph_only += other.graph_only;
    not_embedded += other.not_embedded;
    missing_path += other.missing_path;
    missing_source += other.missing_source;
    unaligned_divergent_source += other.unaligned_divergent_source;
    unaligned_divergent_graph += other.unaligned_divergent_graph;
}

void Aggregate::add(const Outcome& outcome) {
    if (!outcome.statistics) return;
    const auto& value = *outcome.statistics;
    source_sequences += outcome.source_index.has_value();
    graph_traversals += value.graph_bases > 0 || !outcome.source_index;
    source_bases += value.source_bases;
    graph_bases += value.graph_bases;
    source_ambiguous += value.source_ambiguous_bases;
    graph_ambiguous += value.graph_ambiguous_bases;
    ambiguous_columns += value.ambiguous_columns;
    bases.add(value.bases);
    if (value.edit_distance) {
        edit_distance += *value.edit_distance;
        aligned_columns += value.bases.matched + value.bases.substituted +
            value.bases.source_only + value.bases.graph_only;
    }
    completed_alignments += value.alignment_status == AlignmentStatus::complete;
    unavailable_alignments += value.alignment_status == AlignmentStatus::limit_exceeded ||
        value.alignment_status == AlignmentStatus::memory_limit_exceeded;
    ++statuses[outcome.status];
}

AuditResult audit(const AuditRequest& request, const Graph& graph,
                  Cancellation& cancellation, const ProgressCallback& progress) {
    std::vector<Outcome> outcomes;
    std::set<std::string> source_names;
    MemoryBudget budget(request.resources.memory_bytes);
    Queue<SourceJob> queue;
    Queue<AlignmentJob> alignment_queue;
    std::mutex result_mutex, failure_mutex, completion_mutex;
    std::condition_variable completion_changed;
    std::size_t workers_done = 0;
    bool aligner_done = false;
    std::exception_ptr worker_failure;
    const auto fail = [&](std::exception_ptr failure) {
        {
            std::lock_guard lock(failure_mutex);
            if (!worker_failure) worker_failure = failure;
        }
        cancellation.cancel();
        queue.close();
        alignment_queue.close();
    };

    std::thread aligner([&] {
        try {
            while (auto job = alignment_queue.pop(cancellation)) {
                const auto tracked = job->source_record.length + job->outcome.graph_length;
                auto permit = budget.acquire_within_limit(tracked, cancellation);
                if (permit) {
                    auto source = job->source_spool->read(job->source_record);
                    add_statistics(job->outcome, graph, source,
                        request.resources.alignment_max_cells, cancellation);
                } else {
                    add_unaligned_statistics(job->outcome, job->source_record.length,
                        job->source_ambiguous_bases);
                }
                std::lock_guard lock(result_mutex);
                outcomes.push_back(std::move(job->outcome));
            }
        } catch (const Cancelled&) {
        } catch (...) {
            fail(std::current_exception());
        }
        {
            std::lock_guard lock(completion_mutex);
            aligner_done = true;
        }
        completion_changed.notify_all();
    });

    std::vector<std::thread> workers;
    try {
        for (std::size_t index = 0; index < request.resources.threads; ++index) {
            workers.emplace_back([&] {
                try {
                    while (auto job = queue.pop(cancellation)) {
                        Outcome outcome;
                        if (!graph.path_names.contains(job->identifier)) {
                            outcome.identifier = job->identifier;
                            outcome.status = Status::missing_path;
                            outcome.source_length = job->sequence.size();
                        } else {
                            outcome = compare(graph, job->identifier, job->sequence, cancellation);
                        }
                        outcome.source_index = job->source_index;
                        if (outcome.status == Status::missing_path)
                            outcome.source_digest = sequence_digest({
                                reinterpret_cast<const std::uint8_t*>(job->sequence.data()),
                                job->sequence.size()});
                        if (request.statistics == StatisticsMode::comprehensive &&
                            outcome.status == Status::divergent) {
                            auto spool = std::make_shared<Spool>(request.resources.temp_dir);
                            const auto record = spool->append({job->sequence.data(), job->sequence.size()});
                            const auto source_ambiguous_bases = ambiguous(job->sequence);
                            job->sequence.clear();
                            job->sequence.shrink_to_fit();
                            job->permit = MemoryPermit{};
                            if (!alignment_queue.push({std::move(outcome), std::move(spool),
                                record, source_ambiguous_bases}, cancellation)) break;
                        } else {
                            if (request.statistics == StatisticsMode::comprehensive)
                                add_statistics(outcome, graph, job->sequence,
                                    request.resources.alignment_max_cells, cancellation);
                            std::lock_guard lock(result_mutex);
                            outcomes.push_back(std::move(outcome));
                        }
                    }
                } catch (const Cancelled&) {
                } catch (...) {
                    fail(std::current_exception());
                }
                {
                    std::lock_guard lock(completion_mutex);
                    ++workers_done;
                }
                completion_changed.notify_all();
            });
        }
    } catch (...) {
        fail(std::current_exception());
    }

    std::exception_ptr coordinator_failure;
    try {
        for (std::size_t source_index = 0; source_index < request.sources.size(); ++source_index) {
            const auto& spec = request.sources[source_index];
            stream_fasta(spec.path, [&](const std::string& raw, const std::string& sequence) {
                cancellation.check();
                if (progress) progress();
                const auto name = spec.identifier(raw);
                source_names.insert(name);
                auto permit = budget.acquire(sequence.size(), cancellation, progress);
                if (!queue.push({source_index, name, sequence, std::move(permit)},
                                cancellation, progress))
                    cancellation.check();
            }, progress);
        }
    } catch (const Cancelled&) {
    } catch (...) {
        coordinator_failure = std::current_exception();
        cancellation.cancel();
    }

    queue.close();
    const auto wait_with_progress = [&](const auto& complete) {
        std::unique_lock lock(completion_mutex);
        while (!complete()) {
            completion_changed.wait_for(lock, std::chrono::milliseconds(25));
            lock.unlock();
            if (progress) progress();
            lock.lock();
        }
    };
    const auto wait_without_progress = [&](const auto& complete) {
        std::unique_lock lock(completion_mutex);
        completion_changed.wait(lock, complete);
    };
    try {
        if (!coordinator_failure)
            wait_with_progress([&] { return workers_done == workers.size(); });
    } catch (...) {
        coordinator_failure = std::current_exception();
        cancellation.cancel();
        queue.close();
        alignment_queue.close();
    }
    wait_without_progress([&] { return workers_done == workers.size(); });
    for (auto& worker : workers) worker.join();
    alignment_queue.close();
    try {
        if (!coordinator_failure) wait_with_progress([&] { return aligner_done; });
    } catch (...) {
        coordinator_failure = std::current_exception();
        cancellation.cancel();
        alignment_queue.close();
    }
    wait_without_progress([&] { return aligner_done; });
    aligner.join();
    if (worker_failure) std::rethrow_exception(worker_failure);
    if (coordinator_failure) std::rethrow_exception(coordinator_failure);
    cancellation.check();

    for (const auto& name : graph.path_names) {
        if (!source_names.contains(name)) {
            auto outcome = missing_source(graph, name, cancellation);
            if (request.statistics == StatisticsMode::basic) outcome.statistics.reset();
            outcomes.push_back(std::move(outcome));
        }
    }
    std::sort(outcomes.begin(), outcomes.end(), [](const auto& left, const auto& right) {
        return left.identifier < right.identifier;
    });

    AuditResult result;
    result.outcomes = std::move(outcomes);
    result.memory = budget.telemetry();
    for (const auto& outcome : result.outcomes) {
        result.summary.identical += outcome.status == Status::identical;
        result.summary.divergent += outcome.status == Status::divergent;
        result.summary.missing_path += outcome.status == Status::missing_path;
        result.summary.missing_source += outcome.status == Status::missing_source;
    }
    if (request.statistics == StatisticsMode::comprehensive) {
        AggregateStatistics statistics;
        statistics.by_source.resize(request.sources.size());
        for (const auto& outcome : result.outcomes) {
            statistics.global.add(outcome);
            if (outcome.source_index) statistics.by_source[*outcome.source_index].add(outcome);
            if (outcome.record_type) statistics.by_record_type[*outcome.record_type].add(outcome);
        }
        result.statistics = std::move(statistics);
    }
    return result;
}

}
