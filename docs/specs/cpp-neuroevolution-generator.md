# Spec: C++ Neuroevolution Generator

## Goal
Create a reusable C++20 command-line system that learns a supervised problem from numeric CSV data by evolving multilayer neural-network architectures and parameters.

## Current State
The repository is empty. The host is macOS 26.6.2 on an Apple M5 Pro with Apple Clang 21 and CMake 4.3.1.

## Desired State
The project can load classification or regression data, split and normalize it without validation/test leakage, evolve candidate networks, report validation progress, evaluate the selected network once on held-out test data, save the winning genome, and reproduce runs from a seed.

## Constraints
- Core implementation must be C++.
- Build must work with Apple Clang and CMake.
- Apple Neural Engine access is mediated by Core ML; there are no public C++ ANE instructions.
- Candidate topologies change frequently, so the search uses Apple Accelerate on CPU and parallel candidate evaluation.
- Initial data format is numeric CSV with target column(s) at the end.
- This is a general supervised learner, not a guarantee that arbitrary data contains enough information to solve an arbitrary problem.

## Assumptions
- Classification labels are numeric values in the final CSV column.
- Regression uses one or more final target columns.
- Dense feed-forward networks are an acceptable initial search space.

## Plan
1. Implement CSV loading, deterministic train/validation/test splitting, and train-only feature normalization.
2. Implement dense-network genomes, topology-preserving rebuilds, forward inference, metrics, serialization, and Apple Accelerate matrix-vector operations.
3. Implement tournament selection, elitism, crossover, parameter mutation, topology mutation, diversity preservation, and parallel fitness evaluation.
4. Implement a CLI with help, dry-run, explicit configuration, progress output, and model saving.
5. Add unit/integration tests, sample XOR data, documentation, benchmark, and task record.

## Files Changed
- `CMakeLists.txt`: project build, test, benchmark, and Accelerate configuration.
- `include/neuroevo/neuroevo.hpp`: public API.
- `src/neuroevo.cpp`: dataset, inference, evolution, and persistence.
- `src/main.cpp`: train/predict CLI.
- `tests/test_neuroevo.cpp`: automated tests.
- `tools/benchmark.cpp`: backend benchmark.
- `examples/xor.csv`: example dataset.
- `README.md`: user documentation.
- `docs/blueprints/neuroevolution-system.md`: design blueprint.
- `docs/benchmarks/m5-pro-neuroevo-backends.md`: benchmark record.
- `datasets/tasks/cpp-neuroevolution-generator.md`: reusable task record.

## Commands Used
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/neuroevo --data examples/xor.csv --task classification
```

## Verification
- [x] Accelerate Release build completed without warnings.
- [x] Scalar Release build completed without warnings.
- [x] Unit/integration test executable passed through CTest.
- [x] AddressSanitizer and UndefinedBehaviorSanitizer test build passed.
- [x] End-to-end XOR training reached 1.0 validation and test accuracy for the recorded run.
- [x] Saved genome reloaded and predicted raw row `0,1` as class `1`.
- [x] Dry-run CSV/configuration validation passed.

## Benchmark
On the M5 Pro dense `32x64x64x4` benchmark, the hybrid Accelerate backend median was 1,055,500 inferences/second and scalar C++ median was 272,899 inferences/second, approximately a 3.87x improvement. Full results are in `docs/benchmarks/m5-pro-neuroevo-backends.md`.

## Rollback
All additions are isolated to this new repository. Remove the files introduced by this spec or revert their eventual commit.

## Reuse Points
- Dataset pipeline
- Dense genome and serialization
- Fitness evaluator interface
- Evolution engine and mutation operators
- Accelerate backend abstraction
