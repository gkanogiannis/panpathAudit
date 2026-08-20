test_that("random small alignments match dynamic programming", {
    distance <- function(left, right) {
        n <- nchar(left)
        m <- nchar(right)
        matrix <- matrix(0L, n + 1L, m + 1L)
        matrix[, 1L] <- 0:n
        matrix[1L, ] <- 0:m
        if (n && m) for (i in seq_len(n)) for (j in seq_len(m)) {
            matrix[i + 1L, j + 1L] <- min(
                matrix[i, j + 1L] + 1L,
                matrix[i + 1L, j] + 1L,
                matrix[i, j] + (substr(left, i, i) != substr(right, j, j))
            )
        }
        matrix[n + 1L, m + 1L]
    }
    set.seed(1729)
    for (iteration in seq_len(100)) {
        left <- paste0(sample(c("A", "C", "G", "T"), sample(0:10, 1), TRUE), collapse = "")
        right <- paste0(sample(c("A", "C", "G", "T"), sample(0:10, 1), TRUE), collapse = "")
        native <- panpathAudit:::.native_align(left, right, 100000)
        expect_equal(native$edit_distance, distance(left, right))
    }
})

test_that("portable spool supports concurrent reads and cleanup", {
    result <- panpathAudit:::.native_spool_test()
    expect_true(result$cleaned)
    expect_false(file.exists(result$path))
})

test_that("native routines are registered explicitly", {
    routines <- getDLLRegisteredRoutines("panpathAudit")$.Call
    expect_setequal(names(routines), c(
        "_panpathAudit_audit", "_panpathAudit_align",
        "_panpathAudit_spool_test"
    ))
    expect_false(unclass(getLoadedDLLs()[["panpathAudit"]])$dynamicLookup)
})

test_that("result accessors and coercion are stable", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nP\tsample\t1+\t*")
    result <- auditGFA(case$fasta, case$gfa)
    expect_s4_class(auditOutcomes(result), "DataFrame")
    expect_s4_class(auditSummary(result), "SimpleList")
    expect_s4_class(auditStatistics(result), "SimpleList")
    expect_s4_class(auditProvenance(result), "SimpleList")
    expect_s4_class(auditTelemetry(result), "SimpleList")
    expect_identical(names(auditOutcomes(result)), c(
        "identifier", "source_index", "status", "record_type",
        "source_start_0based", "source_end_0based_exclusive",
        "source_length", "graph_length", "addressed_source_bases",
        "divergence_kind", "source_position_1based",
        "graph_position_1based", "segment", "orientation",
        "traversal_position_1based", "segment_position_1based",
        "range_length", "walk_length", "source_sha256",
        "addressed_source_sha256", "graph_sha256", "source_blake3",
        "addressed_source_blake3", "graph_blake3",
        "source_ambiguous_bases", "graph_ambiguous_bases",
        "ambiguous_columns", "length_delta", "matched", "substituted",
        "source_only", "graph_only", "not_embedded",
        "missing_path_bases", "missing_source_bases",
        "unaligned_divergent_source", "unaligned_divergent_graph",
        "edit_distance", "alignment_status"
    ))
    expect_identical(names(auditSummary(result)),
        c("counts", "percentages", "total"))
    expect_identical(names(auditTelemetry(result)),
        c("limit_bytes", "peak_tracked_bytes", "oversized_contigs", "threads"))
    expect_equal(summary(result)$counts, auditSummary(result)$counts)
    expect_s3_class(as.data.frame(result), "data.frame")
    expect_output(show(result), "topology validated: no")
})
