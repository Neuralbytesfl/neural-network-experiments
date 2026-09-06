# Task Record: Topology-Aware Scheduling

## Date
2026-09-06

## Task Type
Performance optimization, concurrency, benchmark, native application verification.

## Original Request
Implement the next reusable performance step: group compatible evolving networks
and make complex-model evaluation faster.

## Problem
Confirmed: accelerated candidates were evaluated in population order and allocated
fresh matrix buffers separately for training and validation. Unknown at the start:
whether Accelerate's internal threading made fewer candidate workers faster.

## Environment
Confirmed: Apple M5 Pro arm64, macOS 26.6.2, Apple Clang 21.0.0, C++20 Release,
Apple Accelerate, native SwiftUI application.

## Inputs
- Phase 2 commit `53fd1f2`.
- XOR dataset.
- Generated 10,000-row and 100,000-row, 16-input polynomial datasets.
- Existing Release, scalar, sanitizer, native-app, and live-control workflows.

## Actions Taken
1. Updated the performance spec before implementation.
2. Swept 1–18 candidate workers; 18 remained fastest.
3. Checked the installed Accelerate headers for a public batched SGEMM API.
4. Grouped exact layer-shape topologies into bounded work cohorts.
5. Sorted cohorts by estimated multiply cost and reused thread-local matrix storage.
6. Kept small scalar partitions on the direct low-overhead queue.
7. Added one-worker/four-worker deterministic scheduling coverage.
8. Ran paired benchmarks, three sanitizer modes, a 100,000-row stress run, and two
   native GUI/CLI workflows.

## Files Changed
- `src/neuroevo.cpp`: topology cohorts, cost ordering, and reusable thread-local workspaces.
- `tests/test_neuroevo.cpp`: scheduling determinism coverage.
- `README.md`: runtime design summary.
- Performance spec, benchmark, and this task record.

## Commands Used
See `docs/specs/neuroevo-performance-optimization.md` and
`docs/benchmarks/m5-pro-topology-aware-scheduling.md`.

## Output
Complex-workload median throughput improved from 1,397.48 to 1,458.59 evaluations/s
in paired trials. Observed stress-test peak memory decreased slightly from 146,440,192
to 145,162,240 bytes.

## Verification
Confirmed: Release Accelerate and scalar tests, AddressSanitizer,
UndefinedBehaviorSanitizer, ThreadSanitizer, deterministic scheduling test, signed
app verification, large-data stress run, and two live GUI/CLI workflows passed.

## Benchmark
`docs/benchmarks/m5-pro-topology-aware-scheduling.md`

## Rollback
Revert the phase 3 commit. Model and dataset formats are unchanged.

## Reusable Pattern
Group shape-compatible jobs, split large groups into enough cohorts to preserve
parallelism, run expensive cohorts first, and retain scratch capacity per worker.

## Fine-Tuning Usefulness
High.

## Failure
The first implementation grouped tiny scalar workloads and widened the worker
callback signature. It slowed the 1,000-generation XOR benchmark.

## Why It Failed
Confirmed: grouping overhead dominated four-row evaluations. The wider callback
also added overhead to a path invoked 160,000 times.

## Correction
Topology grouping now runs only when at least one partition uses batched Accelerate,
and scratch ownership is thread-local so the original one-argument worker queue is
preserved.

## Lesson
An optimization policy must include an explicit low-overhead bypass; batching is not
automatically beneficial for tiny jobs.
