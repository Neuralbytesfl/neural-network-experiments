# Task Record: Persistent Workers and Contiguous Data

## Date
2026-09-06

## Task Type
Performance optimization, stress testing, native application reliability.

## Original Request
Continue optimizing the neural-network experiment application after its first
maximum-network performance pass.

## Problem
Confirmed: each evolution generation created and joined operating-system threads,
and each candidate repacked identical dataset rows for Accelerate evaluation.

## Environment
Confirmed: Apple M5 Pro arm64, macOS 26.6.2, Apple Clang 21.0.0, C++20 Release,
Apple Accelerate, native SwiftUI application.

## Inputs
- Published phase 1 commit `1a78126`.
- XOR dataset.
- Generated 16-input polynomial datasets with 10,000 and 100,000 rows.
- Existing CLI-to-GUI JSON control bridge.

## Actions Taken
1. Captured end-to-end phase 1 baselines.
2. Added a run-scoped worker pool with barrier completion and exception capture.
3. Added immutable packed train/validation partitions for Accelerate batches.
4. Stress-tested a 100,000-row dataset and fixed a shutdown-order deadlock.
5. Moved CLI polling ownership from SwiftUI view lifetime to the application model.
6. Rebuilt, signed, and exercised the native app through its CLI bridge twice.
7. Ran interleaved phase 1/phase 2 benchmarks to control for machine conditions.

## Files Changed
- `src/neuroevo.cpp`: worker pool and packed partitions.
- `tests/test_neuroevo.cpp`: worker exception propagation coverage.
- `native/NeuroevoStudio/StudioModel.swift`: retained control task and activity.
- `native/NeuroevoStudio/StudioControl.swift`: durable model-owned polling loop.
- `native/NeuroevoStudio/ContentView.swift`: remove view-owned polling.
- `README.md`: document the runtime design.
- Performance spec, benchmark, task, and error records.

## Commands Used
See `docs/specs/neuroevo-performance-optimization.md` and
`docs/benchmarks/m5-pro-worker-pool-contiguous-data.md`.

## Output
Tiny generation-heavy throughput improved 1.29x. Complex-workload throughput
improved 1.02x in paired trials. The 100,000-row run completed at about 139.7 MiB
maximum resident memory.

## Verification
Confirmed: Release tests, sanitizer tests, app signing verification, large-dataset
stress run, deterministic result comparison, and two full live-control workflows
passed.

## Benchmark
`docs/benchmarks/m5-pro-worker-pool-contiguous-data.md`

## Rollback
Revert the phase 2 commit. Saved-model and dataset formats did not change.

## Reusable Pattern
Keep a bounded pool alive across short parallel phases, publish each phase through a
barrier, and store shared input data in an immutable contiguous layout.

## Fine-Tuning Usefulness
High.

## Notes
No secrets or private training data were used. Generated benchmark artifacts remain
outside version control.
