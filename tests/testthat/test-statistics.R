test_that("comprehensive statistics partition source and graph bases", {
    case <- audit_case(">same\nACGT\n>changed\nAAAA\n>absent\nNN", paste(
        "S\t1\tACGT", "S\t2\tAATA", "S\t3\tGG", "P\tsame\t1+\t*",
        "P\tchanged\t2+\t*", "P\textra\t3+\t*", sep = "\n"))
    result <- auditGFA(case$fasta, case$gfa, statistics = "comprehensive")
    global <- auditStatistics(result)$global
    expect_equal(global$source_bases, 10)
    expect_equal(global$graph_bases, 10)
    expect_equal(global$bases$matched, 7)
    expect_equal(global$bases$substituted, 1)
    expect_equal(global$bases$missing_path, 2)
    expect_equal(global$bases$missing_source, 2)
    expect_equal(global$source_ambiguous_bases, 2)
})

test_that("comprehensive statistics count substitutions and indels", {
    substitution <- audit_case(">changed\nAAAA", "S\t1\tAATA\nP\tchanged\t1+\t*")
    row <- outcome_frame(auditGFA(substitution$fasta, substitution$gfa,
        statistics = "comprehensive"))
    expect_equal(c(row$matched, row$substituted), c(3, 1))
    expect_equal(row$edit_distance, 1)

    deletion <- audit_case(">changed\nACGT", "S\t1\tAGT\nP\tchanged\t1+\t*")
    row <- outcome_frame(auditGFA(deletion$fasta, deletion$gfa,
        statistics = "comprehensive"))
    expect_equal(row$source_only, 1)
    expect_equal(row$graph_only, 0)
    expect_equal(row$length_delta, -1)
})

test_that("clipped walk bases are not embedded", {
    case <- audit_case(">sample#1#chr1\nACGTGT", "S\t1\tAC\nS\t2\tGT\nW\tsample\t1\tchr1\t0\t2\t>1\nW\tsample\t1\tchr1\t4\t6\t>2")
    result <- auditGFA(case$fasta, case$gfa, statistics = "comprehensive")
    expect_equal(auditStatistics(result)$global$bases$matched, 4)
    expect_equal(auditStatistics(result)$global$bases$not_embedded, 2)
})

test_that("alignment limits use unavailable buckets", {
    case <- audit_case(">changed\nAAAA", "S\t1\tTTTT\nP\tchanged\t1+\t*")
    row <- outcome_frame(auditGFA(case$fasta, case$gfa,
        statistics = "comprehensive", alignmentMaxCells = 1))
    expect_equal(row$alignment_status, "limit_exceeded")
    expect_true(is.na(row$edit_distance))
    expect_equal(row$unaligned_divergent_source, 4)
    expect_equal(row$unaligned_divergent_graph, 4)
})

test_that("parallel and single-thread results are deterministic", {
    case <- audit_case(">a\nACGT\n>b\nAAAA\n>c\nNNNN", "S\t1\tACGT\nS\t2\tAATA\nS\t3\tNNNN\nP\ta\t1+\t*\nP\tb\t2+\t*\nP\tc\t3+\t*")
    one <- auditGFA(case$fasta, case$gfa, statistics = "comprehensive", threads = 1)
    four <- auditGFA(case$fasta, case$gfa, statistics = "comprehensive", threads = 4)
    expect_equal(outcome_frame(one), outcome_frame(four))
    expect_equal(as.list(auditStatistics(one)), as.list(auditStatistics(four)))
})

test_that("memory telemetry limits alignment", {
    sequence <- paste(rep("A", 700000), collapse = "")
    graph_sequence <- paste(rep("T", 700000), collapse = "")
    case <- audit_case(paste0(">changed\n", sequence),
        paste0("S\t1\t", graph_sequence, "\nP\tchanged\t1+\t*"))
    result <- auditGFA(case$fasta, case$gfa, statistics = "comprehensive",
        memoryMiB = 1, threads = 2)
    row <- outcome_frame(result)
    expect_equal(row$alignment_status, "memory_limit_exceeded")
    expect_lte(auditTelemetry(result)$peak_tracked_bytes, 700000)
})

test_that("an oversized contig runs alone and is reported", {
    sequence <- paste(rep("A", 1100000), collapse = "")
    case <- audit_case(paste0(">large\n", sequence),
        paste0("S\t1\t", sequence, "\nP\tlarge\t1+\t*"))
    result <- auditGFA(case$fasta, case$gfa, memoryMiB = 1, threads = 4)
    expect_equal(outcome_frame(result)$status, "IDENTICAL")
    expect_equal(auditTelemetry(result)$oversized_contigs, 1)
    expect_equal(auditTelemetry(result)$peak_tracked_bytes, 1100000)
})

test_that("basic mode leaves alignment statistics unavailable", {
    case <- audit_case(">sample\nAC", "S\t1\tAC\nP\tsample\t1+\t*")
    result <- auditGFA(case$fasta, case$gfa)
    row <- outcome_frame(result)
    expect_true(is.na(row$matched))
    expect_true(is.na(row$alignment_status))
    expect_length(auditStatistics(result), 0)
})
