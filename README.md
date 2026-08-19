# panpathAudit

`panpathAudit` checks whether FASTA sequences are preserved by named `P` paths
and coordinate-aware `W` walks in GFA files. It does not validate graph
topology.

```r
library(panpathAudit)

fasta <- system.file("extdata", "example.fa", package = "panpathAudit")
gfa <- system.file("extdata", "example.gfa", package = "panpathAudit")

result <- auditGFA(fasta, gfa)
auditOutcomes(result)
```

Use `statistics = "comprehensive"` for base-level alignment statistics.
