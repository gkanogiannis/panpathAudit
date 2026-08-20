\name{auditGFA}
\alias{auditGFA}
\title{Audit FASTA sequences against GFA traversals}
\usage{
auditGFA(
  fasta,
  gfa,
  mapping = NULL,
  statistics = c("basic", "comprehensive"),
  threads = 1L,
  memoryMiB = 1024L,
  alignmentMaxCells = 10000000
)
}
\arguments{
\item{fasta}{Character vector of FASTA paths.}
\item{gfa}{One GFA path.}
\item{mapping}{Optional data frame with one row per FASTA. See Details.}
\item{statistics}{Either \code{"basic"} or \code{"comprehensive"}.}
\item{threads}{Positive worker count.}
\item{memoryMiB}{Positive tracked-memory limit in MiB.}
\item{alignmentMaxCells}{Positive comprehensive-alignment limit.}
}
\value{A \linkS4class{PanPathAuditResult}.}
\description{
Compares source sequences with named GFA \code{P} paths and coordinate-aware
\code{W} walks. Graph topology is not validated.
}
\details{
Gzip input is detected from file content. GFA \code{P} records must use blunt
overlaps (\code{*} or \code{0M}). \code{W} identifiers are formed as
\code{sample#haplotype#sequence}; their ranges are zero-based and half-open.

Mapping modes are \code{exact}, \code{pansn}, and \code{prefix}. PanSN rows
require \code{sample} and numeric \code{haplotype}; prefix rows require
\code{prefix}.

Argument, input, and operational failures raise conditions with classes
\code{panpathAudit_argument_error}, \code{panpathAudit_input_error}, and
\code{panpathAudit_operational_error}. Each inherits from
\code{panpathAudit_error} and carries a \code{diagnostics} \code{DataFrame}.
Diagnostic positions are one-based.
}
\examples{
fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")
result <- auditGFA(fasta, gfa)
auditOutcomes(result)
}
