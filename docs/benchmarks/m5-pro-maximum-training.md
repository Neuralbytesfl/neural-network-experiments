# Benchmark: M5 Pro Maximum-Topology Training

## Date
2026-09-06

## System
- Apple M5 Pro, arm64, 18 logical CPUs (6 performance and 12 efficiency cores)
- macOS 26.6.2 (25G83)
- Apple Clang 21.0.0
- Release build with Apple Accelerate enabled
- Architecture: `16x128x128x128x128x16`, 53,776 parameters
- Partition: 4,096 deterministic classification samples
- Sequential workload: 8 complete partition evaluations
- Parallel workload: 64 complete partition evaluations on 18 workers

## Baseline

Three trials before changing the evaluation path:

| Trial | Sequential samples/s | Parallel samples/s |
|---:|---:|---:|
| 1 | 351,363 | 2,501,330 |
| 2 | 345,668 | 2,390,840 |
| 3 | 341,104 | 2,317,900 |

Median: 345,668 sequential samples/s and 2,390,840 parallel samples/s.

The portable scalar median was 26,296 sequential samples/s and 362,943 parallel
samples/s.

## Change
- Evaluate genomes without copying their layer and parameter storage.
- Reuse prediction workspaces instead of allocating vectors for every layer of
  every sample.
- On Apple Accelerate builds, evaluate partitions in bounded chunks using SGEMM
  matrix-matrix operations rather than dispatching SGEMV once per sample/layer.
- Cap each batched workspace at about one million floats so large datasets do not
  create an unbounded allocation per worker.
- Add a configurable maximum-topology benchmark and a numerical-equivalence test.

## After

Five Apple Accelerate trials after the final bounded-batch implementation:

| Trial | Sequential samples/s | Parallel samples/s |
|---:|---:|---:|
| 1 | 3,240,890 | 9,629,110 |
| 2 | 3,288,740 | 8,924,520 |
| 3 | 3,264,060 | 8,664,000 |
| 4 | 2,936,080 | 7,801,680 |
| 5 | 2,641,860 | 7,313,310 |

Median: 3,240,890 sequential samples/s and 8,664,000 parallel samples/s.

The portable scalar median after workspace reuse was 64,140 sequential samples/s
and 888,561 parallel samples/s.

## Result
- Apple Accelerate sequential partition evaluation: **9.38x faster**.
- Apple Accelerate 18-worker population evaluation: **3.62x faster**.
- Portable scalar sequential evaluation: **2.44x faster**.
- Portable scalar 18-worker population evaluation: **2.45x faster**.
- Benchmark metric checksums were unchanged at printed precision (`22.7111`
  sequential and `182.096` parallel).

An application-level run generated 10,000 polynomial rows with 16 features and
trained 128 candidates for 53 generations (6,784 evaluations) before patience
stopped the run. CLI-observed wall time was 5.49 seconds. The winner had topology
`16 -> 33 -> 1`, 595 parameters, validation score `-0.633454`, and test score
`-0.664671`. The GUI bridge and standalone C++ CLI both predicted `0.347718` for
the same all-zero row.

## Profile Evidence
The baseline five-second sample attributed 1,177 of 2,549 samples under the main
evaluation frame to allocator `free`, with additional allocation and copy frames
inside `Network::predict`. After workspace reuse, the equivalent profile contained
no allocator frames in the hot call tree; 1,912 of 2,133 evaluation samples were
inside Accelerate SGEMV before batching was applied. This supported replacing
per-row calls with bounded SGEMM batches.

## Memory Check
A 100,000-row, 18-worker maximum-topology run completed at 6.39 million sample
evaluations/s with a peak footprint of about 201 MB. Workspaces are chunked, so
their size does not grow without bound with dataset row count.

## Notes
- Results are local measurements and include normal thermal/system-load variation.
- The five final trials slowed over time, likely from thermal or concurrent-load
  effects; medians are reported rather than selecting the fastest value.
- Apple Accelerate is CPU execution. It is not evidence of Neural Engine use.
- The evolutionary run intentionally stressed model size; its short search budget
  was a performance test, not a claim of production model quality.
- Apple AddressSanitizer does not support `detect_leaks=1`; address and undefined
  behavior checks passed after disabling that unsupported option.
