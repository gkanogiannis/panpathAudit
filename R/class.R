setClass(
    "PanPathAuditResult",
    slots = c(
        outcomes = "DataFrame",
        summary = "SimpleList",
        statistics = "SimpleList",
        provenance = "SimpleList",
        telemetry = "SimpleList"
    )
)

setValidity("PanPathAuditResult", function(object) {
    required <- c("identifier", "source_index", "status", "record_type")
    missing <- setdiff(required, names(object@outcomes))
    if (!length(missing)) return(TRUE)
    paste("missing outcome columns:", paste(missing, collapse = ", "))
})

.PanPathAuditResult <- function(
    outcomes, summary, statistics, provenance, telemetry
) {
    object <- new(
        "PanPathAuditResult",
        outcomes = DataFrame(outcomes, check.names = FALSE),
        summary = SimpleList(summary),
        statistics = SimpleList(statistics),
        provenance = SimpleList(provenance),
        telemetry = SimpleList(telemetry)
    )
    validObject(object)
    object
}

auditOutcomes <- function(x) x@outcomes
auditSummary <- function(x) x@summary
auditStatistics <- function(x) x@statistics
auditProvenance <- function(x) x@provenance
auditTelemetry <- function(x) x@telemetry

setMethod("show", "PanPathAuditResult", function(object) {
    counts <- object@summary$counts
    cat("PanPathAuditResult\n")
    cat(
        "  identical:", counts[["identical"]],
        " divergent:", counts[["divergent"]], "\n"
    )
    cat(
        "  missing path:", counts[["missing_path"]],
        " missing source:", counts[["missing_source"]], "\n"
    )
    cat("  topology validated: no\n")
})

setMethod("summary", "PanPathAuditResult", function(object, ...) {
    SimpleList(
        counts = object@summary$counts,
        percentages = object@summary$percentages,
        total = object@summary$total,
        statistics = object@statistics
    )
})

setMethod(
    "as.data.frame", "PanPathAuditResult",
    function(x, row.names = NULL, optional = FALSE, ...) {
        as.data.frame(
            x@outcomes, row.names = row.names, optional = optional, ...
        )
    }
)
