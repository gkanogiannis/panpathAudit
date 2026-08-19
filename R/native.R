.panpath_audit_native <- function(request) {
    .Call("_panpathAudit_audit", request, PACKAGE = "panpathAudit")
}

.native_align <- function(source, graph, max_cells = 1e7) {
    .Call(
        "_panpathAudit_align", source, graph, max_cells,
        PACKAGE = "panpathAudit"
    )
}

.native_spool_test <- function(directory = tempdir()) {
    .Call("_panpathAudit_spool_test", directory, PACKAGE = "panpathAudit")
}
