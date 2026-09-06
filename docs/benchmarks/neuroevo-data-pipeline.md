# Benchmark: Neuroevo Data Pipeline

## Date
2026-09-06

## System
Apple M5 Pro MacBook Pro, 18 CPU cores, 24 GB memory, macOS 26.6.2. Release build with Apple Clang and Apple Accelerate enabled.

## Baseline
No built-in generation, profiling, or cleaning pipeline existed, so there is no comparable before measurement.

## Change
Added deterministic synthetic CSV generation, structural profiling, and conservative cleaning.

## After
One run of `./build/neuroevo_data_benchmark` on 100,000 rows and three columns:

| Operation | Seconds | Rows/second |
|---|---:|---:|
| Generate spiral | 0.0639 | 1,565,720 |
| Profile CSV | 0.1069 | 935,472 |
| Clean/write copy | 0.1361 | 734,676 |

The existing inference benchmark measured 1,050,050 inferences/second for a `32x64x64x4` network in the same verification run.

## Result
The added data pipeline is fast enough for interactive use at the tested size. It runs outside the per-generation candidate-evaluation hot path.

## Notes
This is one local run, not a statistical performance claim. Disk cache, storage load, compiler versions, row width, and cleaning issues affect results. Re-run the benchmark on target hardware before setting product limits.
