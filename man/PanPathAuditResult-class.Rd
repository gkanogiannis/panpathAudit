\name{PanPathAuditResult-class}
\alias{PanPathAuditResult-class}
\alias{auditOutcomes}
\alias{auditSummary}
\alias{auditStatistics}
\alias{auditProvenance}
\alias{auditTelemetry}
\alias{show,PanPathAuditResult-method}
\alias{summary,PanPathAuditResult-method}
\alias{as.data.frame,PanPathAuditResult-method}
\title{Audit result class}
\usage{
auditOutcomes(x)
auditSummary(x)
auditStatistics(x)
auditProvenance(x)
auditTelemetry(x)
}
\arguments{
\item{x}{A \code{PanPathAuditResult}.}
}
\value{
\code{auditOutcomes()} returns a \code{DataFrame}. Other accessors return
\code{SimpleList} objects. \code{as.data.frame()} returns the outcomes as a
base data frame.
}
\description{
Stores traversal outcomes, summary counts, optional comprehensive statistics,
provenance, and memory telemetry.
}
\section{Outcome columns}{
\describe{
\item{\code{identifier}}{Mapped path or walk identifier.}
\item{\code{source_index}}{One-based FASTA index.}
\item{\code{status}}{\code{IDENTICAL}, \code{DIVERGENT},
\code{MISSING_PATH}, or \code{MISSING_SOURCE}.}
\item{\code{record_type}}{\code{P} or \code{W}.}
\item{\code{source_start_0based}, \code{source_end_0based_exclusive}}{
Walk source range.}
\item{\code{source_length}, \code{graph_length},
\code{addressed_source_bases}}{Sequence and addressed-range sizes.}
\item{\code{divergence_kind}}{\code{BASE}, \code{END}, or
\code{RANGE_LENGTH}.}
\item{\code{source_position_1based}, \code{graph_position_1based}}{
First divergent base position.}
\item{\code{segment}, \code{orientation}}{Segment and traversal strand at
the divergence.}
\item{\code{traversal_position_1based},
\code{segment_position_1based}}{Divergence coordinates within the traversal
and segment.}
\item{\code{range_length}, \code{walk_length}}{Expected and reconstructed
sizes for a walk range mismatch.}
\item{\code{source_sha256}, \code{addressed_source_sha256},
\code{graph_sha256}}{SHA-256 digests.}
\item{\code{source_blake3}, \code{addressed_source_blake3},
\code{graph_blake3}}{BLAKE3 digests.}
\item{\code{source_ambiguous_bases}, \code{graph_ambiguous_bases},
\code{ambiguous_columns}}{IUPAC ambiguity counts.}
\item{\code{length_delta}}{Graph length minus addressed source length.}
\item{\code{matched}, \code{substituted}, \code{source_only},
\code{graph_only}}{Aligned base counts.}
\item{\code{not_embedded}, \code{missing_path_bases},
\code{missing_source_bases}}{Unrepresented base counts.}
\item{\code{unaligned_divergent_source},
\code{unaligned_divergent_graph}}{Divergent bases not aligned because of a
resource limit.}
\item{\code{edit_distance}}{Edit distance when alignment completed.}
\item{\code{alignment_status}}{\code{not_applicable}, \code{not_required},
\code{complete}, \code{limit_exceeded}, or
\code{memory_limit_exceeded}.}
}
Columns that do not apply contain \code{NA}.
}
\section{Accessor structures}{
\code{auditSummary()} contains \code{counts}, \code{percentages}, and
\code{total}. \code{auditStatistics()} is empty in basic mode; otherwise it
contains \code{global}, \code{by_source}, and \code{by_record_type} aggregates.
Each aggregate reports sequence and base totals, ambiguity counts, edit and
alignment totals, base-category counts, and status counts.

\code{auditProvenance()} records package and reference versions, normalized
input paths, mapping and resource options, compression, GFA version and record
counts, overlap interpretation, and the false \code{topology_validated} flag.

\code{auditTelemetry()} contains \code{limit_bytes},
\code{peak_tracked_bytes}, \code{oversized_contigs}, and \code{threads}.
}
\examples{
fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")
result <- auditGFA(fasta, gfa)
summary(result)
}
