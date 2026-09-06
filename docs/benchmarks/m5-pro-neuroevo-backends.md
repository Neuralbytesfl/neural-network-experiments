# Benchmark: M5 Pro Neuroevolution Backends

## Date
2026-09-06

## System
- Apple M5 Pro, arm64
- macOS 26.6.2 (25G83)
- Apple Clang 21.0.0
- Release build (`-DCMAKE_BUILD_TYPE=Release`)
- Architecture: `32x64x64x4`, 6,532 parameters
- Workload: 409,600 single-row inferences per trial

## Baseline
Portable scalar C++, three trials:

| Trial | Seconds | Inferences/second |
|---:|---:|---:|
| 1 | 1.50101 | 272,883 |
| 2 | 1.50084 | 272,913 |
| 3 | 1.50092 | 272,899 |

Median: 272,899 inferences/second.

## Change
Use a hybrid backend: scalar loops below 256 multiply-accumulates per layer and Apple Accelerate CBLAS for larger layers.

## After
Hybrid Apple Accelerate, three trials:

| Trial | Seconds | Inferences/second |
|---:|---:|---:|
| 1 | 0.384775 | 1,064,520 |
| 2 | 0.388062 | 1,055,500 |
| 3 | 0.389885 | 1,050,570 |

Median: 1,055,500 inferences/second.

## Result
The hybrid backend was approximately 3.87x faster than the scalar backend for this dense-network workload.

The end-to-end XOR search is dominated by tiny layers, threading, and orchestration. A prior direct-Accelerate version was about 5% slower there, which motivated the scalar threshold. This is CPU acceleration, not Neural Engine execution.

## Notes
- Results are local measurements, not universal M5 performance claims.
- Thermal state and other processes were not controlled.
- The checksum matched across backends (`-88115.4` as printed), providing a coarse result-consistency check.
- Re-run `neuroevo_benchmark` on the deployment machine before tuning the threshold.

