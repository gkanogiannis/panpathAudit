# Native engine provenance

The initial engine was ported from `panpath-audit` and preserves its 0.1.2
behavioral contract.

- Rust repository commit : `ad06a3a20987c2ef5c6eef84bd19b4a44cf86ce9`
- Rust repository: <https://github.com/gkanogiannis/panpath-audit>
- Rust reference tag: `v0.1.2`
- Package port date: 2026-08-20

Current R/C++ package and Rust reference versions are independent. The engine validates sequence correspondence, not graph topology.

# Port and third-party provenance

The native engine in `src/` is a C++ port of the Rust implementation at the
commit above. The vendored BLAKE3 implementation is version
`1.8.2`, from <https://github.com/BLAKE3-team/BLAKE3>, and is distributed under
the Apache-2.0 and CC0 notices in `inst/third-party/`. The vendored SHA-256
implementation and its redistribution terms are documented in
`inst/third-party/SHA256-LICENSE.md`.

Development of the package included assistance from OpenAI Codex
and Anthropic Claude. The maintainer reviewed the generated and ported code,
tested the package, and assumes responsibility for its correctness and
maintenance.

# Example data provenance

The FASTA and GFA files in `inst/extdata` are synthetic. Reproduce them from
the package root with `Rscript inst/scripts/make-extdata.R`. No external data
or preprocessing is involved.
