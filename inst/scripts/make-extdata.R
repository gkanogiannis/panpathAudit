# Synthetic fixtures; run from the package root.

output <- file.path("inst", "extdata")
dir.create(output, recursive = TRUE, showWarnings = FALSE)

fixtures <- list(
    "example.fa" = c(">sample", "ACGT"),
    "example.gfa" = c(
        "H\tVN:Z:1.1", "S\t1\tAC", "S\t2\tGT",
        "P\tsample\t1+,2+\t*"
    ),
    "walk.fa" = c(">sample#1#chr1", "ACGTGT"),
    "walk.gfa" = c(
        "H\tVN:Z:1.1", "S\t1\tAC", "S\t2\tGT",
        "W\tsample\t1\tchr1\t0\t2\t>1",
        "W\tsample\t1\tchr1\t4\t6\t>2"
    )
)

for (name in names(fixtures)) {
    value <- paste0(paste(fixtures[[name]], collapse = "\n"), "\n")
    writeBin(charToRaw(value), file.path(output, name))
}
