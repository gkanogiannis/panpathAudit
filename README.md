# panpathAudit

[![R-CMD-check](https://github.com/gkanogiannis/panpathAudit/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/gkanogiannis/panpathAudit/actions/workflows/R-CMD-check.yaml)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)

Audit whether FASTA sequences are preserved by named `P` paths and
coordinate-aware `W` walks in GFA files.

A pangenome graph usually claims to embed the assemblies it was built from.
Construction, normalization, chunked builds, coordinate liftover and format
round-trips can quietly break that claim while leaving the graph perfectly well
formed, so structural validators stay quiet. `panpathAudit` answers a narrower
question: **is the source sequence still recoverable from the traversal, and if
not, exactly where does it stop matching?**

It does not validate graph topology — links, non-blunt overlaps and connectivity
are out of scope.

## Installation

To install `panpathAudit` as an R package:
```r
if (!requireNamespace("BiocManager", quietly = TRUE))
    install.packages("BiocManager")

BiocManager::install("panpathAudit")
```

You can install the development version of `panpathAudit` R package like so:
```r
devtools::install_github("gkanogiannis/panpathAudit")
```

Building the package requires a C++20 compiler and zlib.

## Quick start

```r
library(panpathAudit)

fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")

result <- auditGFA(fasta, gfa)
result
#> PanPathAuditResult
#>   identical: 1  divergent: 0
#>   missing path: 0  missing source: 0
#>   topology validated: no

auditOutcomes(result)[, c("identifier", "status", "record_type")]
#> DataFrame with 1 row and 3 columns
#>    identifier      status record_type
#>   <character> <character> <character>
#> 1      sample   IDENTICAL           P
```

Each source record and each named traversal gets exactly one of four statuses:
`IDENTICAL`, `DIVERGENT`, `MISSING_PATH`, or `MISSING_SOURCE`. All four are
completed outcomes, not errors.

## Locating a divergence

The shipped `example.fa` holds `ACGT`. Audited against a graph whose path spells
`ACGA` instead, the report pins the exact base that broke:

```r
broken <- tempfile(fileext = ".gfa")
writeLines(c(
    "H\tVN:Z:1.1", "S\t1\tAC", "S\t2\tGA", "P\tsample\t1+,2+\t*"
), broken)

auditOutcomes(auditGFA(fasta, broken))[, c(
    "status", "divergence_kind", "source_position_1based",
    "segment", "segment_position_1based"
)]
#> DataFrame with 1 row and 5 columns
#>        status divergence_kind source_position_1based     segment
#>   <character>     <character>              <numeric> <character>
#> 1   DIVERGENT            BASE                      4           2
#>   segment_position_1based
#>                 <numeric>
#> 1                       2
```

Base 4 of the source, contributed by segment `2`, at position 2 within that
segment. Reverse-strand steps additionally report `orientation` and a separate
traversal coordinate.

## Features

- GFA `P` paths and coordinate-aware `W` walks, with blunt overlaps.
- `exact`, PanSN, and `prefix` mapping between FASTA headers and traversal names.
- Plain or gzip-compressed input, detected from file content.
- SHA-256 and BLAKE3 digests of source, addressed source, and graph sequences.
- Optional base-level alignment statistics (`statistics = "comprehensive"`) under
  explicit memory and work bounds, with per-run telemetry.
- Results and diagnostics as Bioconductor `DataFrame` / `SimpleList` containers.
- Provenance capturing inputs, options, GFA version, and record counts.

## Coordinates

Columns named `*_0based` retain GFA's zero-based, half-open ranges. Every other
reported position, including all divergence coordinates, is one-based. Column
names carry the convention so the two are never ambiguous.

## Error handling

Failures raise conditions inheriting from `panpathAudit_error` and carry a
structured `diagnostics` `DataFrame` rather than only a message:

```r
tryCatch(
    auditGFA("missing.fa", "graph.gfa"),
    panpathAudit_error = function(error) error$diagnostics
)
```

## Documentation

```r
vignette("panpathAudit")   # Auditing GFA traversals
?auditGFA
?PanPathAuditResult
```

## Citation

```r
citation("panpathAudit")
```

Gkanogiannis, A. (2026). *Panpath-Audit: Sequence-Level Verification of Embedded
Pangenome Paths Against Source Assemblies*. Preprints.
DOI [10.20944/preprints202608.1872.v1](https://doi.org/10.20944/preprints202608.1872.v1)

## License

Apache License 2.0. Vendored BLAKE3 and SHA-256 implementations retain their own
notices in [`inst/third-party/`](inst/third-party); see
[`inst/PROVENANCE.md`](inst/PROVENANCE.md) for engine and example-data
provenance.

Bug reports and questions:
<https://github.com/gkanogiannis/panpathAudit/issues>
