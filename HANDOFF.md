# `panpathAudit` C++ and Bioconductor handoff

## Purpose

This directory is the staging area for an R/Bioconductor package that exposes
the sequence-preservation audit implemented by
[`panpath-audit`](https://github.com/gkanogiannis/panpath-audit). The recommended
R package and GitHub repository name is **`panpathAudit`**: it preserves the tool
identity, is descriptive, and avoids confusion with the existing CRAN package
`GFA` (group factor analysis).

The initial package should audit source FASTA sequences against named `P` paths
and coordinate-aware `W` walks in GFA1-family files. It should not claim to
validate graph topology.

## Snapshot provenance

- Copied from: `/mnt/d/agkanogiannis/Sync_Research/panpath-audit/cpp`
- Handoff date: 2026-08-19
- Rust reference release: `panpath-audit` 0.1.2, tag `v0.1.2`
- Rust repository commit at handoff: `ad06a3a20987c2ef5c6eef84bd19b4a44cf86ce9`
- Snapshot location here: `cpp-port/`

The source repository currently ignores `/cpp/`, so the C++ snapshot is not
identified by the Rust commit and can be lost unless it is committed in this new
repository. Generated CMake build directories were deliberately not copied.

## What is in `cpp-port`

The snapshot is a standalone CMake C++20 project:

- `include/panpath/`: public core types for input, graph indexing, audit results,
  statistics, memory accounting, digests, CLI configuration, and reports.
- `src/`: implementation plus the `panpath-audit-cpp` command-line frontend.
- `tests/`: native core checks, small fixtures, and Rust/C++ output-conformance
  checks.
- `vendor/`: BLAKE3, SHA-256, and nlohmann/json sources with their license files.
- `CMakeLists.txt`: builds `panpath_core`, the CLI, and tests against zlib.

The core is compact and sensibly modular. It uses RAII for temporary storage,
memory permits, input handles, and digest state. Audit outcomes are structured
C++ values, work is bounded by a memory budget, and per-source comparison is
parallelized.

## Verified state at handoff

On Linux with GCC 15.2 and zlib 1.3.1:

- The normal CMake build succeeds.
- The native core test and all nine Rust/C++ conformance tests pass (10/10).
- Identical, divergent, coordinate-aware walk, comprehensive human/JSON/TSV,
  and invalid-input cases agree with the Rust executable for the included
  fixtures.
- Compilation reports two `-Wmisleading-indentation` warnings in
  `src/statistics.cpp`; these should be fixed before adopting warnings-as-errors.

An AddressSanitizer/UndefinedBehaviorSanitizer build compiles, but its tests
cannot be interpreted in the current execution environment: LeakSanitizer exits
fatally because it is running under ptrace, and the conformance subprocesses
therefore fail before producing reports. Re-run sanitizer tests in ordinary
Linux CI rather than treating this as a detected leak.

The current tests are only a starting point. `tests/core_tests.cpp` relies on
`assert`, which can be compiled out when `NDEBUG` is set. The conformance suite
covers nine scenarios, while the Rust CLI suite covers sixty.

## Important gaps before an R package

### 1. Cross-platform temporary storage

`src/spool.cpp` is POSIX-only. It includes `<unistd.h>` and calls `mkstemp`,
`unlink`, `pread`, and `pwrite`; it will not compile on Windows. Replace this
with a portable random-access temporary-file abstraction. The R wrapper should
pass an R-session temporary directory, and cleanup must remain RAII-based on
Linux, macOS, and Windows. Named-file cleanup on Windows is acceptable when
unlink-while-open semantics are unavailable.

### 2. Separate the engine from the CLI

The reusable API is still shaped around `Config`, which mixes file sources,
CLI mappings, report format, resources, and scratch paths. `audit_basic()` is
also misnamed because comprehensive work is selected through `Config`.

Create an engine-facing request type containing only audit inputs and resource
policy. Keep CLI parsing and human/JSON/TSV rendering as reference adapters, not
as dependencies of the R interface. Expose aggregate statistics as structured
C++ values rather than reconstructing them inside the JSON renderer.

### 3. Structured diagnostics

Parsing and operational failures are primarily encoded as strings. Introduce a
diagnostic type with at least `category`, `code`, and `message`, plus optional
context fields. Convert diagnostics to R conditions or result-table rows at the
R boundary. Never allow a C++ exception to cross `.Call`; translate it into a
clear R error.

### 4. R interruption and threads

No R API may be called from worker threads. Add cooperative cancellation owned
by the core and check for R user interrupts only from the calling/main thread.
Cancellation must close queues, join the aligner and workers, release temporary
files, and then raise the R interruption.

### 5. Testing and versioning

- Replace assertion-only native tests with checks that remain active in release
  builds.
- Port the behavioral coverage of the Rust 60-test CLI suite, especially input
  errors, mapping, ranges, memory limits, alignment limits, and structured output.
- Add randomized small-alignment comparison and cross-platform temporary-file
  tests.
- Keep Rust 0.1.1 as the initial golden oracle, but make fixtures and expected
  structured results self-contained so the installed R package never requires
  Rust.
- Resolve the CMake project version (`0.1.0`) versus the Rust reference version
  (`0.1.1`). Package and report-schema versioning should be deliberately
  separated for the R interface.

## Recommended R interface

Start with file paths; direct `DNAStringSet` support can be added after the
file-based native path is stable.

```r
auditGFA(
    fasta,
    gfa,
    mapping = NULL,
    statistics = c("basic", "comprehensive"),
    threads = 1L,
    memoryMiB = 1024L,
    alignmentMaxCells = 10000000
)
```

Recommended semantics:

- `fasta`: one or more paths.
- `gfa`: one path.
- `mapping`: per-FASTA exact, PanSN, or prefix mapping specification.
- Return an S4 `PanPathAuditResult`, not generated JSON parsed back into R.
- Store per-traversal outcomes in an `S4Vectors::DataFrame`.
- Store summary counts, provenance, memory telemetry, and optional aggregate
  statistics in documented slots or `S4Vectors::SimpleList` values.
- Provide `show()`, `summary()`, `as.data.frame()`, and accessors without
  inventing a linear genomic coordinate class for graph segment positions.
- Keep one-based diagnostic positions visible to R users; retain explicitly
  named zero-based half-open source-range fields where the GFA convention must
  be preserved.

Use a thin Rcpp boundary. The C++ workers must operate only on native data, with
R objects created after worker completion on the main thread.

## Recommended package structure

The final repository default branch must contain only the R package. After the
portability/API pass, migrate selected files from `cpp-port/` into the normal R
layout:

```text
panpathAudit/
  DESCRIPTION
  NAMESPACE
  R/
  src/
  inst/include/panpath/        # only if headers are intentionally public
  inst/extdata/                # tiny FASTA/GFA examples
  tests/testthat/
  vignettes/
```

Compile through `src/Makevars` and `src/Makevars.win`, not through CMake during
package installation. Target R 4.6.0 or newer and retain C++20, which is the
default package standard from R 4.6. Use Rcpp for the native boundary and decide
whether zlib should come from the supported R/Bioconductor toolchain or
`zlibbioc` after testing all three builders.

Preserve and declare the licenses of all vendored code. Consider removing
nlohmann/json from the installed core if JSON is retained only for standalone
conformance, since the R interface should return native structured data.

## Suggested implementation order

1. Initialize this directory as the `panpathAudit` R package repository and
   commit the untouched `cpp-port/` snapshot plus this handoff.
2. Fix active compiler warnings and replace release-disabled native assertions.
3. Make `Spool` portable and establish Linux, Windows, and macOS C++ CI.
4. Refactor engine request/result/diagnostic types away from CLI/report concerns.
5. Expand Rust parity tests until the critical Rust CLI behaviors are covered.
6. Add the thin Rcpp wrapper and `PanPathAuditResult` representation.
7. Add R tests, runnable examples, and an evaluated BiocStyle vignette.
8. Run `R CMD build`, `R CMD check`, and `BiocCheck` on the Bioconductor devel
   baseline and address every error, warning, and note before submission.

## Useful references

- Bioconductor package names:
  <https://contributions.bioconductor.org/package-name.html>
- Package submissions:
  <https://contributions.bioconductor.org/bioconductor-package-submissions.html>
- Compiled code:
  <https://contributions.bioconductor.org/other-than-Rcode.html>
- Documentation and vignettes:
  <https://contributions.bioconductor.org/docs.html>
- Build/check/BiocCheck:
  <https://contributions.bioconductor.org/build-check-bioccheck.html>
- R 4.6.0 C++20 change:
  <https://stat.ethz.ch/CRAN/doc/manuals/r-release/NEWS.pdf>

## Standalone snapshot commands

From the `panpathAudit` repository root, with the Rust reference binary already
built elsewhere:

```bash
cmake -S cpp-port -B /tmp/panpathAudit-cpp-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DRUST_BINARY=/absolute/path/to/panpath-audit/target/release/panpath-audit
cmake --build /tmp/panpathAudit-cpp-build --parallel
ctest --test-dir /tmp/panpathAudit-cpp-build --output-on-failure
```

Do not commit the temporary build directory.
