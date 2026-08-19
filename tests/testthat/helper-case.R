audit_case <- function(fasta, gfa) {
    directory <- case_directory()
    fasta_path <- file.path(directory, "source.fa")
    gfa_path <- file.path(directory, "graph.gfa")
    writeLines(fasta, fasta_path, useBytes = TRUE)
    writeLines(gfa, gfa_path, useBytes = TRUE)
    list(fasta = fasta_path, gfa = gfa_path, directory = directory)
}

case_directory <- function() {
    directory <- tempfile("panpathAudit-case-")
    dir.create(directory)
    directory
}

write_gzip <- function(path, text) {
    connection <- gzfile(path, "wb")
    on.exit(close(connection))
    writeLines(text, connection, useBytes = TRUE)
}

outcome_frame <- function(result) as.data.frame(auditOutcomes(result))

captured_error <- function(expression) {
    tryCatch(force(expression), error = identity)
}
