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

test_that("result accessors and coercion are stable", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nP\tsample\t1+\t*")
    result <- auditGFA(case$fasta, case$gfa)
    expect_s4_class(auditOutcomes(result), "DataFrame")
    expect_s4_class(auditSummary(result), "SimpleList")
    expect_s4_class(auditStatistics(result), "SimpleList")
    expect_s4_class(auditProvenance(result), "SimpleList")
    expect_s4_class(auditTelemetry(result), "SimpleList")
    expect_equal(summary(result)$counts, auditSummary(result)$counts)
    expect_s3_class(as.data.frame(result), "data.frame")
    expect_output(show(result), "topology validated: no")
})
