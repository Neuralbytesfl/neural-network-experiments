# Benchmark: Persistent Workers and Contiguous Data

## Date
2026-09-06

## System
- Apple M5 Pro, arm64
- macOS 26.6.2
- Apple Clang 21.0.0
- C++20 Release build with Apple Accelerate
- 18 candidate-evaluation workers

## Baseline
The baseline is published phase 1 commit `1a78126`. Baseline and phase 2 binaries
were run alternately on the same machine and dataset to reduce thermal and system
load bias.

| Workload | Trials | Median elapsed | Median throughput |
| --- | ---: | ---: | ---: |
| Tiny XOR, phase 1 | 5 | 0.37418 s | 427,605.31 evaluations/s |
| Complex polynomial, phase 1 | 3 | 5.69299 s | 1,349.03 evaluations/s |

The tiny workload used population 160 for 1,000 generations. The complex workload
used population 128 for 60 generations, 16 inputs, initial width 512, and topology
mutation rate 1.

## Change
- Retain one bounded worker pool throughout an evolution run.
- Pack training and validation features/targets once into immutable contiguous arrays.
- Feed the first Accelerate layer directly from the packed feature array.
- Join workers explicitly before synchronization members are destroyed.
- Keep the GUI control loop owned by the application model rather than a SwiftUI view.

## After

| Workload | Trials | Median elapsed | Median throughput | Change |
| --- | ---: | ---: | ---: | ---: |
| Tiny XOR, phase 2 | 5 | 0.28995 s | 551,825.15 evaluations/s | 1.29x throughput |
| Complex polynomial, phase 2 | 3 | 5.57702 s | 1,377.08 evaluations/s | 1.02x throughput |

The final winner, scores, parameter counts, and evaluation counts matched between
phase 1 and phase 2 at printed precision. The complex result is a modest 2.1%
throughput improvement; candidate matrix math remains the dominant cost there.

## Large Dataset Memory Check
A generated 100,000-row, 16-input polynomial dataset used a 70,000/15,000/15,000
train/validation/test split. One generation with population 18 completed with:

- 0.39 s wall time
- 0.05997 s reported evolution time
- 146,440,192 bytes maximum resident set (about 139.7 MiB)
- 18 evaluations at 300.16 evaluations/s

## Verification
- Release unit tests passed.
- AddressSanitizer and UndefinedBehaviorSanitizer tests passed with Apple leak detection disabled.
- The 100,000-row stress workload completed after the shutdown-order fix.
- The signed arm64 app passed release verification.
- Two consecutive live CLI workflows passed generation, profiling, training, prediction, navigation, and status assertions.

## Result
Improved. Persistent workers materially reduce overhead for small, generation-heavy
runs. Contiguous caches provide a smaller gain when large matrix operations dominate.

## Notes
An initial stress run exposed a nondeterministic shutdown deadlock caused by member
destruction order. The pool now joins its workers explicitly while its mutex and
condition variables are still alive. Benchmark numbers are local measurements, not
general guarantees for every dataset or thermal condition.
