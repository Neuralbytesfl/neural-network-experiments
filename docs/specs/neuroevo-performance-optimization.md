# Spec: Neuroevolution Performance Optimization

## Goal
Measure the largest supported dense-network workload on the Apple M5 Pro, identify the dominant CPU and allocation costs, and improve training/evaluation throughput without changing model semantics or deterministic results.

Phase 2 extends this work by retaining evaluation workers across generations and
prepacking partition features/targets once per run instead of once per candidate.

Phase 3 investigates topology-aware scheduling and candidate batching. It must
first verify which public Accelerate primitives are available and measure worker
scaling so the implementation does not add a nominal batch abstraction that makes
the real workload slower.

## Current State
Phase 1 removed genome copies and repeated layer-output allocation, added bounded Accelerate batch evaluation, and introduced a maximum-topology benchmark. Phase 2 retained workers across generations and packed each data partition once per run. Before phase 3, accelerated candidates were still scheduled in population order and their matrix workspaces were allocated separately for every training and validation evaluation.

## Desired State
- A reproducible maximum-topology benchmark that can exercise both inference and candidate evaluation.
- Measurements that separate single-network inference from parallel evolutionary evaluation.
- Reduced avoidable copying and allocation in the dominant path.
- Passing unit tests and numerically consistent benchmark checksums/scores.
- Before/after measurements captured in `docs/benchmarks/`.
- A bounded reusable worker pool with clean shutdown and exception propagation.
- Contiguous partition caches shared read-only by candidate workers.
- Topology-aware work queues that reuse capacity and reduce scheduling imbalance.
- A measured policy for avoiding candidate-thread/BLAS oversubscription.

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
7. Capture an end-to-end evolutionary baseline for both tiny and complex datasets.
8. Add a run-scoped worker pool so generation barriers do not recreate OS threads.
9. Add run-scoped contiguous train/validation caches consumed by batched evaluation.
10. Repeat correctness, sanitizer, native-app, and end-to-end performance checks.
11. Sweep worker counts and profile topology diversity against the phase 2 commit.
12. Group compatible candidates into cost-ordered work units and reuse per-worker batch storage.
13. Add an adaptive concurrency policy only if paired measurements justify it.
14. Repeat deterministic, sanitizer, stress-memory, CLI/GUI, and paired performance checks.

## Files Changed
- `docs/specs/neuroevo-performance-optimization.md`: working specification and reproducibility record.
- `CMakeLists.txt`: registers the maximum-topology training benchmark.
- `.gitignore`: excludes the reproducible sanitizer build directory.
- `tools/benchmark_training.cpp`: deterministic configurable evaluation workload.
- `src/neuroevo.cpp`: borrowed evaluation, bounded Accelerate batches, persistent workers, topology cohorts, cost ordering, and reusable workspaces.
- `tests/test_neuroevo.cpp`: metric equivalence, worker-error propagation, and topology-scheduling determinism tests.
- `native/NeuroevoStudio/StudioModel.swift`: owns the persistent live-control task and activity token.
- `native/NeuroevoStudio/StudioControl.swift`: correct JSON numeric handling, complete status, and a model-owned control loop.
- `native/NeuroevoStudio/ContentView.swift`: no longer owns CLI polling through view lifetime.
- `scripts/test-live-control`: regression coverage for numeric edge settings and elapsed metrics.
- `README.md`: documents the new benchmark.
- `docs/benchmarks/m5-pro-maximum-training.md`: before/after results and profile evidence.
- `docs/benchmarks/m5-pro-worker-pool-contiguous-data.md`: phase 2 paired end-to-end results.
- `docs/benchmarks/m5-pro-topology-aware-scheduling.md`: phase 3 scaling, paired, and memory results.
- `datasets/tasks/neuroevo-performance-optimization.md`: reusable task record.
- `datasets/tasks/neuroevo-worker-pool-contiguous-data.md`: phase 2 task record.
- `datasets/tasks/neuroevo-topology-aware-scheduling.md`: phase 3 task record.
- `datasets/errors/neuroevo-worker-pool-shutdown-deadlock.md`: failure and correction found by stress testing.

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

```bash
./build/neuroevo --data build/complex-polynomial.csv --header --task regression \
  --population 128 --generations 60 --threads 18 --initial-hidden 512 \
  --topology-rate 1 --target-score 1 --patience 60 \
  --output build/phase3-complex.neuroevo
cmake -S . -B /tmp/neuroevo-tsan-build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
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

Phase 2 used interleaved runs against the published phase 1 commit. Persistent
workers improved the tiny 1,000-generation workload from 427,605 to 551,825
evaluations/s (1.29x), while the complex workload improved from 1,349 to 1,377
evaluations/s (1.02x). A 100,000-row stress run peaked at 146,440,192 resident
bytes. See `docs/benchmarks/m5-pro-worker-pool-contiguous-data.md`.

Phase 3 used interleaved runs against phase 2 commit `53fd1f2`. On the complex
workload, median throughput improved from 1,397.48 to 1,458.59 evaluations/s
(1.044x). The tiny scalar workload stayed within 1.5% measurement variation and
continues to bypass topology grouping. See
`docs/benchmarks/m5-pro-topology-aware-scheduling.md`.

## Rollback
Revert the performance commit or restore the touched files from Git history. The optimization must not require a data or saved-model migration.

## Reuse Points
- Configurable benchmark workloads for future hardware comparisons.
- Allocation-free evaluation workspace for other dense-network workloads.
- Documented guidance for selecting scalar versus Accelerate execution thresholds.
- Reusable fixed worker pool with barrier completion and exception propagation.
- Immutable run-scoped packed partitions for many-candidate evaluation.
- Cost-ordered topology cohorts that preserve enough tasks for worker balancing.
- Thread-local Accelerate workspaces reused across candidates and partitions.
