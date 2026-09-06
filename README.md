# Neuroevo C++

A dependency-light C++20 neuroevolution engine for numeric supervised-learning data. It evolves dense-network weights, biases, hidden-layer widths/depth, and activation functions, selects against a validation partition, and evaluates the winner once against a held-out test partition.

This program can search for a model that generalizes from examples. It cannot guarantee an accurate solution to every problem: the data must contain a learnable relationship, the train/validation/test distributions must be representative, and the search budget and supported network family must be suitable.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS, CMake links Apple Accelerate automatically. Disable it for comparison:

```bash
cmake -S . -B build-scalar -DCMAKE_BUILD_TYPE=Release \
  -DNEUROEVO_ENABLE_ACCELERATE=OFF
```

## Data

Input is numeric CSV. Feature columns come first and target column(s) last.

Classification example:

```csv
x1,x2,label
0,0,0
0,1,1
```

Labels may be any finite numeric values. Regression may have multiple final target columns. Feature and regression-target statistics are learned only from the training partition. The saved model contains those statistics for raw-value prediction.

## Train and predict

```bash
./build/neuroevo \
  --data examples/xor.csv \
  --header \
  --task classification \
  --population 200 \
  --generations 200 \
  --seed 42 \
  --output winner.neuroevo

./build/neuroevo \
  --model winner.neuroevo \
  --predict-row "0,1"
```

Inspect all controls:

```bash
./build/neuroevo --help
./build/neuroevo --data examples/xor.csv --header --dry-run
```

The reported regression score is negative mean-squared error, so higher is better and zero is ideal. Configure a regression stop threshold accordingly, such as `--target-score -0.01`.

## Evolution design

- Genomes are serializable data; runnable networks are constructed from them.
- Tournament selection uses training fitness plus a small parameter-count penalty.
- Elites survive unchanged; offspring receive uniform crossover and mutation.
- Mutations affect parameters, activations, hidden widths, and hidden depth.
- Random immigrants reduce premature population collapse.
- Validation score/loss chooses the saved winner; test data is not used by evolution.
- Candidate evaluation is parallel and deterministic for a fixed seed apart from ordinary floating-point/backend differences.

This is a compact topology-evolving genetic algorithm, not a complete NEAT implementation: crossover occurs directly when parent shapes match, and unlike NEAT it does not yet use innovation numbers or species.

## Apple M5 acceleration

The hot dense layers use a hybrid path: small matrices stay in a low-overhead scalar loop, while larger matrices use Apple Accelerate BLAS. Candidate networks are evaluated in parallel across CPU cores.

Apple does not expose Neural Engine instructions as a public C++ intrinsic set. Neural Engine scheduling happens through Core ML. Core ML's `.all` or `.cpuAndNeuralEngine` compute-unit configuration makes a compatible deployed model eligible for the Neural Engine, but the operating system decides graph placement. The rapidly changing candidate graphs are therefore evolved with Accelerate; a production deployment can convert the selected model to an ML Program and load it through Core ML. Do not describe the Accelerate path as Neural Engine execution.

Apple references:

- [Core ML compute units](https://developer.apple.com/documentation/coreml/mlcomputeunits)
- [Accelerate and BNNS](https://developer.apple.com/documentation/Accelerate/bnns-library)
- [Core ML Tools ML Program format](https://apple.github.io/coremltools/docs-guides/source/target-conversion-formats.html)

## Benchmark

```bash
./build/neuroevo_benchmark
./build-scalar/neuroevo_benchmark
```

The benchmark fixes its random seed and runs a `32x64x64x4` network. See `docs/benchmarks/m5-pro-neuroevo-backends.md` for captured results.

## Limits and next extensions

- Numeric tabular data and dense feed-forward networks only
- No categorical/text/image preprocessing
- No gradient fine-tuning after evolution
- No NEAT speciation or innovation-number crossover
- No direct export of the saved text artifact to Core ML yet

The `Dataset`, `Genome`, `Network`, evaluator, and evolution engine are separate so these can be extended without rewriting the CLI.

