# Benchmark: Topology-Aware Scheduling

## Date
2026-09-06

## System
- Apple M5 Pro, arm64
- macOS 26.6.2
- Apple Clang 21.0.0
- C++20 Release with Apple Accelerate
- Baseline commit: `53fd1f2`

## Baseline Investigation
A 20-generation complex-workload sweep confirmed that candidate concurrency still
scales through all 18 logical workers:

| Workers | Evaluations/s |
| ---: | ---: |
| 1 | 138.05 |
| 2 | 280.03 |
| 4 | 516.59 |
| 8 | 687.23 |
| 12 | 750.49 |
| 14 | 773.19 |
| 18 | 779.42 |

This ruled out reducing the worker cap as an optimization. The installed public
Accelerate CBLAS headers do not expose a multiple-weight batched SGEMM operation,
so phase 3 groups scheduling and storage while retaining one SGEMM per candidate
layer.

## Change
- Build exact layer-shape topology keys for accelerated candidates.
- Split topology groups into bounded cohorts, leaving at least two scheduling waves
  per worker when a topology dominates the population.
- Schedule estimated high-cost cohorts first to reduce end-of-generation tail time.
- Reuse thread-local Accelerate input/output vectors across candidates and across
  training/validation partitions.
- Bypass grouping for small scalar partitions where map construction costs more than
  it saves.

## Paired Result
Phase 2 and phase 3 binaries were run alternately. The complex benchmark used five
trials per binary; the tiny benchmark used five trials per binary.

| Workload | Phase 2 median | Phase 3 median | Result |
| --- | ---: | ---: | ---: |
| Complex polynomial | 1,397.48 evaluations/s | 1,458.59 evaluations/s | **1.044x** |
| Complex elapsed | 5.49561 s | 5.26537 s | **4.2% lower** |
| Tiny XOR | 559,568.71 evaluations/s | 551,370.42 evaluations/s | 1.5% lower |

The tiny workload has only four rows, never enters Accelerate topology grouping, and
completes in about 0.29 seconds. Its small difference is treated as measurement/code
layout variation rather than a useful regression signal. An early version grouped
all workloads and was rejected after it measurably slowed this case.

Scores, winner IDs/topologies, parameter counts, and evaluation counts remained
unchanged at printed precision. A dedicated test also compared one-worker and
four-worker evolution and selected the same winner.

## Large Dataset Check
The same 100,000-row, 16-input polynomial stress workload completed with:

- 0.22 s wall time
- 0.06336 s reported evolution time
- 145,162,240 bytes maximum resident set (about 138.4 MiB)
- Identical printed winner scores and 2,305-parameter topology

Phase 2 used 146,440,192 maximum resident bytes on this check, so retained workspace
capacity did not increase observed peak memory.

## Verification
- Release Accelerate tests passed.
- Release scalar tests passed without compiler warnings.
- AddressSanitizer and UndefinedBehaviorSanitizer passed.
- ThreadSanitizer passed the complete test executable.
- Signed arm64 application verification passed.
- Two consecutive live CLI/GUI workflows passed.

## Result
Improved for the intended complex Accelerate workload. The gain comes from lower
allocation churn, topology-local scheduling, and reduced heavy-candidate tail time.

## Notes
Apple's Neural Engine is still not directly programmable from this dynamic C++
evolution loop. The existing Core ML export remains the appropriate path for making
a selected, fixed winner eligible for Neural Engine deployment.
