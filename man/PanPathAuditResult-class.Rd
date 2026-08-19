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
\examples{
fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")
result <- auditGFA(fasta, gfa)
summary(result)
}
