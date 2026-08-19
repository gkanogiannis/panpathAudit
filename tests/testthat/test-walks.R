test_that("full and coordinate-aware walks reconstruct PanSN sources", {
    full <- audit_case(">sample#1#chr1\nACGT", "H\tVN:Z:1.1\nS\t1\tAC\nS\t2\tGT\nW\tsample\t1\tchr1\t*\t*\t>1>2")
    expect_equal(outcome_frame(auditGFA(full$fasta, full$gfa))$status, "IDENTICAL")

    clipped <- audit_case(">sample#1#chr1\nACGTGT", "H\tVN:Z:1.1\nS\t1\tAC\nS\t2\tGT\nW\tsample\t1\tchr1\t0\t2\t>1\nW\tsample\t1\tchr1\t4\t6\t>2")
    row <- outcome_frame(auditGFA(clipped$fasta, clipped$gfa))
    expect_equal(row$status, "IDENTICAL")
    expect_equal(row$source_start_0based, 0)
    expect_equal(row$source_end_0based_exclusive, 6)
    expect_equal(row$addressed_source_bases, 4)
    expect_equal(row$addressed_source_sha256, row$graph_sha256)
    expect_false(row$source_sha256 == row$graph_sha256)
})

test_that("walk ranges are stitched in coordinate order", {
    case <- audit_case(">sample#1#chr1\nACGT", "S\t1\tAC\nS\t2\tGT\nW\tsample\t1\tchr1\t2\t4\t>2\nW\tsample\t1\tchr1\t0\t2\t>1")
    expect_equal(outcome_frame(auditGFA(case$fasta, case$gfa))$status, "IDENTICAL")
})

test_that("walk correspondence errors are structured", {
    overlap <- audit_case(">sample#1#chr1\nACGT", "S\t1\tAC\nW\tsample\t1\tchr1\t0\t2\t>1\nW\tsample\t1\tchr1\t1\t3\t>1")
    error <- captured_error(auditGFA(overlap$fasta, overlap$gfa))
    expect_equal(error$diagnostics$category[error$diagnostics$code == "W_RANGE_OVERLAP"], "correspondence")

    duplicate <- audit_case(">sample#1#chr1\nAC", "S\t1\tAC\nP\tsample#1#chr1\t1+\t*\nW\tsample\t1\tchr1\t*\t*\t>1")
    expect_true("DUPLICATE_EMBEDDED_PATH" %in%
        captured_error(auditGFA(duplicate$fasta, duplicate$gfa))$diagnostics$code)

    ambiguous <- audit_case(">sample#1#chr1\nACAC", "S\t1\tAC\nW\tsample\t1\tchr1\t*\t*\t>1\nW\tsample\t1\tchr1\t*\t*\t>1")
    expect_true("AMBIGUOUS_W_RANGES" %in%
        captured_error(auditGFA(ambiguous$fasta, ambiguous$gfa))$diagnostics$code)
})

test_that("walk fields and bounds are validated", {
    invalid_haplotype <- audit_case(">sample#x#chr1\nAC", "S\t1\tAC\nW\tsample\tx\tchr1\t0\t2\t>1")
    expect_true("INVALID_HAPLOTYPE" %in%
        captured_error(auditGFA(invalid_haplotype$fasta, invalid_haplotype$gfa))$diagnostics$code)

    bounds <- audit_case(">sample#1#chr1\nAC", "S\t1\tAC\nW\tsample\t1\tchr1\t0\t3\t>1")
    expect_true("W_RANGE_OUT_OF_BOUNDS" %in%
        captured_error(auditGFA(bounds$fasta, bounds$gfa))$diagnostics$code)

    truncated <- audit_case(">sample#1#chr1\nAC", "S\t1\tAC\nW\tsample\t1\tchr1\t0\t2")
    expect_true("MALFORMED_W_RECORD" %in%
        captured_error(auditGFA(truncated$fasta, truncated$gfa))$diagnostics$code)
})

test_that("walk length mismatches identify the range", {
    case <- audit_case(">sample#1#chr1\nACGT", "S\t1\tACG\nW\tsample\t1\tchr1\t0\t2\t>1")
    row <- outcome_frame(auditGFA(case$fasta, case$gfa))
    expect_equal(row$status, "DIVERGENT")
    expect_equal(row$divergence_kind, "RANGE_LENGTH")
    expect_equal(row$range_length, 2)
    expect_equal(row$walk_length, 3)
})

test_that("walk provenance reports versions and record counts", {
    case <- audit_case(">sample#1#chr1\nAC", "H\tVN:Z:1.0\nS\t1\tAC\nW\tsample\t1\tchr1\t*\t*\t>1")
    provenance <- auditProvenance(auditGFA(case$fasta, case$gfa))
    expect_equal(provenance$gfa_version, "1.0")
    expect_equal(provenance$path_records, 0)
    expect_equal(provenance$walk_records, 1)
})

test_that("walk divergence retains range and segment coordinates", {
    case <- audit_case(">sample#1#chr1\nAT", "S\t1\tAC\nW\tsample\t1\tchr1\t0\t2\t>1")
    row <- outcome_frame(auditGFA(case$fasta, case$gfa))
    expect_equal(row$record_type, "W")
    expect_equal(row$source_start_0based, 0)
    expect_equal(row$source_end_0based_exclusive, 2)
    expect_equal(row$source_position_1based, 2)
    expect_equal(row$segment, "1")
    expect_equal(row$segment_position_1based, 2)
})

test_that("missing-source walks retain record type and length", {
    case <- audit_case(">present\nAC", "S\t1\tAN\nW\textra\t1\tchr1\t*\t*\t>1>1\nP\tpresent\t1+\t*")
    rows <- outcome_frame(auditGFA(case$fasta, case$gfa, statistics = "comprehensive"))
    extra <- rows[rows$identifier == "extra#1#chr1", ]
    expect_equal(extra$status, "MISSING_SOURCE")
    expect_equal(extra$record_type, "W")
    expect_equal(extra$graph_length, 4)
    expect_equal(extra$missing_source_bases, 4)
})
