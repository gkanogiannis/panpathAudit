# panpathAudit

`panpathAudit` checks whether FASTA sequences are preserved by named `P` paths
and coordinate-aware `W` walks in GFA files. It does not validate graph
topology.

Install the development version from Bioconductor with:

```r
if (!requireNamespace("BiocManager", quietly = TRUE))
    install.packages("BiocManager")
BiocManager::install("panpathAudit")
```

```r
library(panpathAudit)

fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")

result <- auditGFA(fasta, gfa)
auditOutcomes(result)
```

Use `statistics = "comprehensive"` for base-level alignment statistics.

Inputs may be plain or gzip-compressed. The parser supports GFA `P` paths and
`W` walks with blunt overlaps. Range fields retain GFA's zero-based, half-open
coordinates; displayed base positions are one-based.

Failures carry a structured `diagnostics` `DataFrame`:

```r
tryCatch(
    auditGFA("missing.fa", "graph.gfa"),
    panpathAudit_error = function(error) error$diagnostics
)
```
