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
\item{mapping}{Optional data frame with one row per FASTA and columns
\code{mode}, \code{sample}, \code{haplotype}, and \code{prefix}. Modes are
\code{exact}, \code{pansn}, and \code{prefix}.}
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
Input and operational failures raise conditions inheriting from
\code{panpathAudit_error}. Their \code{diagnostics} member is a
\code{DataFrame}. Source range columns ending in \code{0based} or
\code{0based_exclusive} retain GFA coordinates. Diagnostic positions are
one-based.
}
\examples{
fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")
result <- auditGFA(fasta, gfa)
auditOutcomes(result)
}
