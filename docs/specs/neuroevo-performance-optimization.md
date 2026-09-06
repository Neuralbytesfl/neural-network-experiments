# Spec: Neuroevolution Performance Optimization

## Goal
Measure the largest supported dense-network workload on the Apple M5 Pro, identify the dominant CPU and allocation costs, and improve training/evaluation throughput without changing model semantics or deterministic results.

## Current State
The engine evaluates candidates in parallel and uses a hybrid scalar/Apple Accelerate matrix-vector path. Each `evaluate` call currently constructs a `Network` by copying the genome layers, and each single-row prediction allocates and moves layer-output vectors. The existing benchmark covers a `32x64x64x4` inference network but not the maximum supported four hidden layers of width 128 or end-to-end population evaluation.

## Desired State
- A reproducible maximum-topology benchmark that can exercise both inference and candidate evaluation.
- Measurements that separate single-network inference from parallel evolutionary evaluation.
- Reduced avoidable copying and allocation in the dominant path.
- Passing unit tests and numerically consistent benchmark checksums/scores.
- Before/after measurements captured in `docs/benchmarks/`.

## Constraints
- macOS 26.6.2 on Apple M5 Pro arm64.
- C++20 and the existing dependency-light design.
- Apple Accelerate may be used; Apple Neural Engine scheduling is not directly available to arbitrary C++ training loops.
- Public APIs and saved-model compatibility must remain intact.
- Benchmark runs must remain bounded so the interactive application is not monopolized for an excessive period.

## Assumptions
- The maximum supported topology is four hidden layers, each 128 neurons wide.
- A synthetic in-memory partition is sufficient to isolate evaluation throughput.
- The existing fixed seed and output checksum can detect large semantic regressions; unit tests provide broader functional coverage.

## Plan
1. Extend the benchmark harness to cover the maximum topology and full partition evaluation.
2. Build Release scalar and Accelerate variants and capture repeated baseline trials.
3. Profile the maximum workload using available macOS profiling tools and allocation instrumentation.
4. Remove confirmed avoidable copies and per-sample/per-layer allocations while retaining the public `predict` interface.
5. Rebuild, run tests, compare deterministic outputs, and repeat identical trials.
6. Record results, caveats, and reusable optimization guidance.

## Files Changed
- `docs/specs/neuroevo-performance-optimization.md`: working specification and reproducibility record.
- `CMakeLists.txt`: registers the maximum-topology training benchmark.
- `.gitignore`: excludes the reproducible sanitizer build directory.
- `tools/benchmark_training.cpp`: deterministic configurable evaluation workload.
- `src/neuroevo.cpp`: borrowed genome evaluation, reusable workspaces, and bounded Accelerate batches.
- `tests/test_neuroevo.cpp`: classification metric equivalence test.
- `native/NeuroevoStudio/StudioControl.swift`: correct JSON numeric handling and complete metric/settings status.
- `scripts/test-live-control`: regression coverage for numeric edge settings and elapsed metrics.
- `README.md`: documents the new benchmark.
- `docs/benchmarks/m5-pro-maximum-training.md`: before/after results and profile evidence.
- `datasets/tasks/neuroevo-performance-optimization.md`: reusable task record.

## Commands Used
Planned reproducible command families:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/neuroevo_benchmark
```

```bash
./build/neuroevo_training_benchmark --dry-run
./build/neuroevo_training_benchmark
./build-scalar/neuroevo_training_benchmark
sample <pid> 5 -file /tmp/neuroevo.sample.txt
./scripts/build-macos-app
./scripts/studioctl generate --pattern polynomial --rows 10000 --inputs 16 --noise 0.1 --seed 4242 --output "$PWD/build/complex-polynomial.csv"
./scripts/studioctl set initialHidden 512
./scripts/studioctl train --output "$PWD/build/complex-polynomial.neuroevo"
./scripts/studioctl wait --training-timeout 180
```

## Verification
- [x] Release build passed
- [x] Unit tests passed in Accelerate and scalar builds
- [x] Maximum-topology benchmark completed
- [x] Before/after workload and build flags matched
- [x] Output checksum and evaluation metrics remained consistent
- [x] End-to-end native-app CLI training and prediction completed
- [x] GUI-compatible C API tests passed
- [x] AddressSanitizer and UndefinedBehaviorSanitizer checks passed with leak detection disabled
- [x] Signed arm64 application verification and live-control self-test passed

## Benchmark
Captured three baseline trials and five final trials. Median 18-worker throughput
improved from 2,390,840 to 8,664,000 sample evaluations/s (3.62x). See
`docs/benchmarks/m5-pro-maximum-training.md` for raw measurements and caveats.

## Rollback
Revert the performance commit or restore the touched files from Git history. The optimization must not require a data or saved-model migration.

## Reuse Points
- Configurable benchmark workloads for future hardware comparisons.
- Allocation-free evaluation workspace for other dense-network workloads.
- Documented guidance for selecting scalar versus Accelerate execution thresholds.
