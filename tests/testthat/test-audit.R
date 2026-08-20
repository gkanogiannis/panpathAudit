test_that("identical and divergent paths report stable outcomes", {
    identical <- audit_case(">sample\nacgt", "S\t1\tAC\nS\t2\tGT\nP\tsample\t1+,2+\t*")
    result <- auditGFA(identical$fasta, identical$gfa)
    row <- outcome_frame(result)
    expect_s4_class(result, "PanPathAuditResult")
    expect_equal(row$status, "IDENTICAL")
    expect_equal(row$record_type, "P")
    expect_equal(row$source_sha256, row$graph_sha256)
    expect_false(auditProvenance(result)$topology_validated)

    divergent <- audit_case(">sample\nacga", "S\t1\tAC\nS\t2\tGT\nP\tsample\t1+,2+\t*")
    row <- outcome_frame(auditGFA(divergent$fasta, divergent$gfa))
    expect_equal(row$status, "DIVERGENT")
    expect_equal(row$source_position_1based, 4)
    expect_equal(row$segment, "2")
    expect_equal(row$segment_position_1based, 2)
})

test_that("reverse traversal reports traversal and segment coordinates", {
    case <- audit_case(">sampleB#1#chr1\nACGTGCAACC",
        "H\tVN:Z:1.0\nS\t1\tACGT\nS\t2\tTTGA\nS\t3\tCC\nP\tsampleB#1#chr1\t1+,2-,3+\t*")
    row <- outcome_frame(auditGFA(case$fasta, case$gfa))
    expect_equal(row$status, "DIVERGENT")
    expect_equal(row$orientation, "-")
    expect_equal(row$source_position_1based, 5)
    expect_equal(row$traversal_position_1based, 1)
    expect_equal(row$segment_position_1based, 4)
})

test_that("reverse traversal supports IUPAC symbols", {
    case <- audit_case(">sample\nNBDHVKMWSRYACGT", "S\t1\tACGTRYSWKMBDHVN\nP\tsample\t1-\t*")
    expect_equal(outcome_frame(auditGFA(case$fasta, case$gfa))$status, "IDENTICAL")
})

test_that("truncated paths report graph exhaustion", {
    case <- audit_case(">sample\nACGT", "S\t1\tACG\nP\tsample\t1+\t*")
    row <- outcome_frame(auditGFA(case$fasta, case$gfa))
    expect_equal(row$divergence_kind, "END")
    expect_equal(row$source_position_1based, 4)
    expect_true(is.na(row$segment))
})

test_that("missing paths and sources are completed outcomes", {
    case <- audit_case(">present\nAC\n>absent\nNN", "S\t1\tAC\nS\t2\tGG\nP\tpresent\t1+\t*\nP\textra\t2+\t*")
    result <- auditGFA(case$fasta, case$gfa)
    rows <- outcome_frame(result)
    expect_equal(rows$status, c("MISSING_PATH", "MISSING_SOURCE", "IDENTICAL"))
    expect_equal(unname(auditSummary(result)$counts[c("missing_path", "missing_source")]), c(1, 1))
})

test_that("multiple FASTAs and mappings form one source set", {
    directory <- case_directory()
    first <- file.path(directory, "first.fa")
    second <- file.path(directory, "second.fa")
    graph <- file.path(directory, "graph.gfa")
    writeLines(">chr1\nAC", first)
    writeLines(">chr2\nGT", second)
    writeLines(c("S\t1\tAC", "S\t2\tGT", "P\tsample#1#chr1\t1+\t*", "P\tpre-chr2\t2+\t*"), graph)
    mapping <- data.frame(
        mode = c("pansn", "prefix"), sample = c("sample", ""),
        haplotype = c("1", ""), prefix = c("", "pre-")
    )
    rows <- outcome_frame(auditGFA(c(first, second), graph, mapping))
    expect_equal(rows$identifier, c("pre-chr2", "sample#1#chr1"))
    expect_true(all(rows$status == "IDENTICAL"))
    expect_equal(sort(rows$source_index), c(1, 2))
})

test_that("duplicate identifiers across FASTAs are rejected", {
    directory <- case_directory()
    first <- file.path(directory, "first.fa")
    second <- file.path(directory, "second.fa")
    graph <- file.path(directory, "graph.gfa")
    writeLines(">dup\nAC", first)
    writeLines(">dup\nGT", second)
    writeLines("S\t1\tAC\nP\tdup\t1+\t*", graph)
    error <- captured_error(auditGFA(c(first, second), graph))
    expect_s3_class(error, "panpathAudit_input_error")
    expect_true("DUPLICATE_SEQUENCE_IDENTIFIER" %in% error$diagnostics$code)
})

test_that("mapping arguments are validated", {
    case <- audit_case(">x\nAC", "S\t1\tAC\nP\tx\t1+\t*")
    expect_error(auditGFA(case$fasta, case$gfa, data.frame(mode = "bad")),
        class = "panpathAudit_argument_error")
    expect_error(auditGFA(case$fasta, case$gfa,
        data.frame(mode = "pansn", sample = "x", haplotype = "h")),
        class = "panpathAudit_argument_error")
    expect_error(auditGFA(case$fasta, case$gfa, threads = 0),
        class = "panpathAudit_argument_error")
    expect_error(auditGFA(case$fasta, case$gfa, memoryMiB = 0),
        class = "panpathAudit_argument_error")
})

test_that("gzip is detected by content", {
    case <- audit_case(">sample\nACGT", "S\t1\tACGT\nP\tsample\t1+\t*")
    write_gzip(case$fasta, ">sample\nACGT")
    write_gzip(case$gfa, "S\t1\tACGT\nP\tsample\t1+\t*")
    result <- auditGFA(case$fasta, case$gfa)
    expect_equal(outcome_frame(result)$status, "IDENTICAL")
    expect_equal(auditProvenance(result)$fasta_compressions, "gzip")
    expect_equal(auditProvenance(result)$gfa_compression, "gzip")
})

test_that("paths with spaces and Unicode are supported", {
    directory <- file.path(case_directory(), "input space \u03b1")
    dir.create(directory)
    fasta <- file.path(directory, "source \u03b2.fa")
    gfa <- file.path(directory, "graph \u03b3.gfa")
    writeLines(">sample\nACGT", fasta, useBytes = TRUE)
    writeLines("S\t1\tACGT\nP\tsample\t1+\t*", gfa, useBytes = TRUE)
    expect_equal(outcome_frame(auditGFA(fasta, gfa))$status, "IDENTICAL")
})

test_that("unavailable input files produce structured diagnostics", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nP\tsample\t1+\t*")
    missing_fasta <- file.path(case$directory, "missing.fa")
    missing_gfa <- file.path(case$directory, "missing.gfa")
    fasta_error <- captured_error(auditGFA(missing_fasta, case$gfa))
    gfa_error <- captured_error(auditGFA(case$fasta, missing_gfa))
    expect_s3_class(fasta_error, "panpathAudit_input_error")
    expect_s3_class(gfa_error, "panpathAudit_input_error")
    expect_true("FASTA_READ" %in% fasta_error$diagnostics$code)
    expect_true("GFA_READ" %in% gfa_error$diagnostics$code)
})

test_that("source and graph preflight errors are combined", {
    case <- audit_case(">dup\nAC\n>dup\nGT", "S\t1\tAX\nP\tdup\tbroken\t*\nS\t2\tAZ")
    error <- captured_error(auditGFA(case$fasta, case$gfa))
    expect_s3_class(error, "panpathAudit_input_error")
    expect_true(all(c("DUPLICATE_SEQUENCE_IDENTIFIER", "INVALID_NUCLEOTIDE",
        "MALFORMED_PATH") %in% error$diagnostics$code))
})

test_that("invalid source symbols include position and symbol", {
    case <- audit_case(">sample\nACX", "S\t1\tAC\nP\tsample\t1+\t*")
    error <- captured_error(auditGFA(case$fasta, case$gfa))
    row <- as.data.frame(error$diagnostics[error$diagnostics$code == "INVALID_SOURCE_NUCLEOTIDE", ])
    expect_equal(row$position, 3)
    expect_equal(row$symbol, "X")
})

test_that("unreconstructable paths block the audit", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nS\t2\t*\nP\tsample\t1+,missing+\t1M")
    error <- captured_error(auditGFA(case$fasta, case$gfa))
    expect_true(all(c("SEQUENCELESS_SEGMENT", "UNSUPPORTED_OVERLAP", "UNRESOLVED_SEGMENT") %in%
        error$diagnostics$code))
})

test_that("invalid unused segments block the audit", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nS\tunused\tAX\nP\tsample\t1+\t*")
    error <- captured_error(auditGFA(case$fasta, case$gfa))
    expect_true("INVALID_NUCLEOTIDE" %in% error$diagnostics$code)
})

test_that("duplicate paths and non-blunt overlaps are rejected", {
    case <- audit_case(">sample\nAC", paste(
        "S\t1\tA", "S\t2\tC", "P\tsample\t1+,2+\t1M",
        "P\tsample\t1+,2+\t*", sep = "\n"))
    codes <- captured_error(auditGFA(case$fasta, case$gfa))$diagnostics$code
    expect_true(all(c("UNSUPPORTED_OVERLAP", "DUPLICATE_PATH") %in% codes))
})

test_that("truncated segment records are rejected", {
    case <- audit_case(">sample\nAC", "S\t1\nP\tsample\t1+\t*")
    expect_true("MALFORMED_S_RECORD" %in%
        captured_error(auditGFA(case$fasta, case$gfa))$diagnostics$code)
})

test_that("resource policy is retained in provenance", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nP\tsample\t1+\t*")
    result <- auditGFA(case$fasta, case$gfa, threads = 3, memoryMiB = 7)
    expect_equal(auditProvenance(result)$threads, 3)
    expect_equal(auditProvenance(result)$memory_mib, 7)
    expect_equal(auditTelemetry(result)$limit_bytes, 7 * 1024^2)
})
