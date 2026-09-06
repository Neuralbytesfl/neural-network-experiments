# Task Record: Neuroevolution Performance Optimization

## Date
2026-09-06

## Task Type
Performance optimization, benchmark, native application verification.

## Original Request
Run the application at a complex network setting, find inefficiencies, optimize it,
and verify the result through the command line.

## Problem
Maximum-topology evaluation spent substantial time copying genome parameters and
allocating/freeing output vectors for every sample and layer. The existing benchmark
did not cover full partition evaluation or the maximum configured topology.

## Environment
Confirmed: Apple M5 Pro arm64, macOS 26.6.2, Apple Clang 21.0.0, C++20 Release
build, Apple Accelerate backend.

## Inputs
- Existing C++ engine, SwiftUI application, C bridge, and live CLI controller.
- Deterministic in-memory maximum-topology benchmark data.
- A generated 10,000-row, 16-feature polynomial dataset for native-app testing.

## Actions Taken
1. Added a configurable maximum-topology training benchmark.
2. Captured Accelerate and scalar baselines.
3. Sampled the running process and confirmed allocator-heavy evaluation.
4. Removed genome copies and reused inference workspaces.
5. Batched Accelerate partition evaluation through bounded SGEMM calls.
6. Fixed Swift JSON numeric `0`/`1` handling in the CLI bridge.
7. Exposed complete performance settings and elapsed time in CLI status.
8. Rebuilt and exercised the native app through generation, training, and prediction.

## Files Changed
- `CMakeLists.txt`: build the training benchmark.
- `.gitignore`: ignore sanitizer build output.
- `tools/benchmark_training.cpp`: configurable deterministic workload.
- `src/neuroevo.cpp`: allocation reuse and bounded Accelerate batches.
- `tests/test_neuroevo.cpp`: evaluation equivalence coverage.
- `native/NeuroevoStudio/StudioControl.swift`: reliable numeric settings and richer status.
- `scripts/test-live-control`: numeric edge-case and elapsed-state regression checks.
- `README.md`: benchmark entry point.
- Performance specification and benchmark documentation.

## Commands Used
See `docs/specs/neuroevo-performance-optimization.md` and
`docs/benchmarks/m5-pro-maximum-training.md`.

## Output
Maximum-topology 18-worker throughput improved from a median 2.39 million to 8.66
million sample evaluations/s. The native app completed and saved a 6,784-evaluation
run, and both prediction entry points agreed.

## Verification
Confirmed: Accelerate and scalar unit tests passed; numerical-equivalence test
passed; application build passed; live generation, training, status, and prediction
commands passed; signed arm64 app verification passed; supported AddressSanitizer
and UndefinedBehaviorSanitizer checks passed; benchmark checksums matched at printed
precision.

## Benchmark
See `docs/benchmarks/m5-pro-maximum-training.md` for raw trials and caveats.

## Rollback
Revert the performance change in Git. No dataset or model migration is required.

## Reusable Pattern
For many-sample dense inference, retain a small scalar/SGEMV path for individual
predictions while batching evaluation partitions into bounded SGEMM workspaces.

## Fine-Tuning Usefulness
High.

## Notes
The generated dataset and trained model are build artifacts and are not committed.
No secrets or private data were used.
