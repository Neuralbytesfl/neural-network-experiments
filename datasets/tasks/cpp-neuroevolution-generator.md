# Task Record: C++ Neuroevolution Generator

## Date
2026-09-06

## Task Type
architecture, machine-learning, C++, benchmark

## Original Request
Build a complete C++ neural-network generator that learns from data and adapts through genetic mutation/evolution, taking advantage of an Apple M5 machine where appropriate.

## Problem
There was no existing project. The requested system needed a concrete definition of “adapt”: generate candidate networks, compare them fairly, evolve their architectures and parameters, and retain a model that generalizes.

## Environment
Confirmed:
- Empty Git repository
- macOS 26.6.2 on Apple M5 Pro
- Apple Clang 21.0.0
- CMake 4.3.1

## Inputs
- User requirements
- Apple documentation for Accelerate, Core ML, and compute-unit selection
- Original NEAT design as architectural background

## Actions Taken
1. Specified the supported supervised-learning and dense-network scope.
2. Added CSV parsing, deterministic splitting, label encoding, and train-only normalization.
3. Added genome construction, validation, inference, metrics, topology mutation, parameter mutation, crossover, tournament selection, elitism, and immigrants.
4. Added parallel evaluation and hybrid Apple Accelerate/scalar matrix math.
5. Added model persistence, raw-row prediction, tests, an example, and benchmark utility.
6. Benchmarked scalar and Accelerate builds on the actual host.

## Files Changed
- `CMakeLists.txt`: build, tests, benchmark, and Apple Accelerate selection
- `include/neuroevo/neuroevo.hpp`: public reusable API
- `src/neuroevo.cpp`: data, network, evolution, and serialization implementation
- `src/main.cpp`: CLI
- `tests/test_neuroevo.cpp`: automated verification
- `tools/benchmark.cpp`: repeatable inference benchmark
- `examples/xor.csv`: small demonstration dataset
- `README.md`: build and operation guide
- `docs/specs/cpp-neuroevolution-generator.md`: working specification
- `docs/blueprints/neuroevolution-system.md`: architecture blueprint
- `docs/benchmarks/m5-pro-neuroevo-backends.md`: measured backend comparison

## Commands Used
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake -S . -B build-scalar -DCMAKE_BUILD_TYPE=Release -DNEUROEVO_ENABLE_ACCELERATE=OFF
cmake --build build-scalar --parallel
./build/neuroevo_benchmark
./build-scalar/neuroevo_benchmark
./build/neuroevo --data examples/xor.csv --header --population 400 --elite 20 --generations 100 --patience 1000 --target-score 2 --seed 314159 --output build/xor-accelerate.neuroevo
./build/neuroevo --model build/xor-accelerate.neuroevo --predict-row "0,1"
```

## Output
A working C++20 library and CLI that evolves and saves classifiers or regressors from numeric CSV data.

## Verification
Confirmed:
- Clean Release build with Accelerate
- Clean Release scalar build
- All automated tests passed
- AddressSanitizer and UndefinedBehaviorSanitizer test build passed
- XOR run achieved validation accuracy 1.0 and held-out test accuracy 1.0 for the recorded seed/run
- Saved model reloaded and predicted class `1` for raw input `0,1`

Not Verified:
- Generalization to a real user dataset
- Core ML/Neural Engine deployment of the saved winner
- Direct power-consumption improvement

## Benchmark
The hybrid Accelerate backend reached a median 1,055,500 inferences/second versus 272,899 for scalar C++ on the recorded dense workload, approximately 3.87x faster.

## Rollback
Revert the files listed above. Build artifacts are ignored and may be removed without affecting source reconstruction.

## Reusable Pattern
Keep the evolvable genotype independent from the executable phenotype, and keep fitness evaluation independent from evolutionary mechanics.

## Fine-Tuning Usefulness
High

## Notes
The Apple Neural Engine has no public direct C++ instruction interface. Core ML is the supported scheduling layer. The implemented Apple Accelerate backend uses optimized CPU kernels and is labeled accordingly.
