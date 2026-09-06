# Neuroevo C++

**A Neuralbytesfl project · Copyright © 2026 Neuralbytesfl · [MIT License](LICENSE)**

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
- Candidate evaluation uses a run-scoped worker pool and is deterministic for a fixed seed apart from ordinary floating-point/backend differences.

This is a compact topology-evolving genetic algorithm, not a complete NEAT implementation: crossover occurs directly when parent shapes match, and unlike NEAT it does not yet use innovation numbers or species.

## Apple M5 acceleration

The hot dense layers use a hybrid path: small matrices stay in a low-overhead scalar loop, while larger matrices use Apple Accelerate BLAS. Candidate networks are evaluated in parallel across CPU cores by persistent workers, and training/validation partitions are packed once per run for batched evaluation. Accelerated candidates are grouped by topology, scheduled largest-first, and reuse thread-local matrix storage; small scalar datasets keep the lower-overhead direct queue.

Apple does not expose Neural Engine instructions as a public C++ intrinsic set. Neural Engine scheduling happens through Core ML. Core ML's `.all` or `.cpuAndNeuralEngine` compute-unit configuration makes a compatible deployed model eligible for the Neural Engine, but the operating system decides graph placement. The rapidly changing candidate graphs are therefore evolved with Accelerate; a production deployment can convert the selected model to an ML Program and load it through Core ML. Do not describe the Accelerate path as Neural Engine execution.

Apple references:

- [Core ML compute units](https://developer.apple.com/documentation/coreml/mlcomputeunits)
- [Accelerate and BNNS](https://developer.apple.com/documentation/Accelerate/bnns-library)
- [Core ML Tools ML Program format](https://apple.github.io/coremltools/docs-guides/source/target-conversion-formats.html)

## Native macOS application

Neuroevo Studio is a native SwiftUI and Swift Charts application backed by the same C++ engine. It includes:

- A guided Create → Prepare → Train → Use workflow
- Reproducible linear, polynomial, sine, XOR, circles, clusters, and spiral generators
- CSV quality profiling, conservative cleaning, visual previews, and cleaning audit reports
- Classification/regression controls
- Live training and validation chart
- Dataset split and shape summary
- Current winning topology
- Generation, validation, test, parameter, evaluation, and elapsed-time metrics
- Cooperative stop that saves the best model found so far
- Run logs and raw-row prediction
- Core ML export and CPU-plus-Neural-Engine-allowed inference
- A searchable embedded manual for data, leakage, evolution, metrics, deployment, and troubleshooting

Build and package the signed arm64 application:

```bash
./scripts/build-macos-app --dry-run
./scripts/build-macos-app
open dist/NeuroevoStudio.app
```

Create and verify an installable DMG:

```bash
./scripts/build-dmg --dry-run
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.dmg
```

For local development, use `./scripts/dev-run --help`. For public distribution, pass a Developer ID Application identity to `build-dmg` and an existing `notarytool` keychain profile; see `docs/runbooks/macos-release.md`. Ad-hoc signing is the default and is intended for local testing.

The app can be preloaded for demos or automation:

```bash
open -n dist/NeuroevoStudio.app --args \
  --data "$PWD/examples/xor.csv" \
  --output "$PWD/build/gui-xor.neuroevo"
```

Add `--page create`, `--page prepare`, `--page train`, `--page use`, or `--page learn` to open a workflow page directly during development and UI testing.

### Live CLI control

When the GUI is running, `studioctl` changes the same observable state used by SwiftUI, so navigation and settings appear in the window within roughly 100 ms:

```bash
./scripts/studioctl status
./scripts/studioctl page create
./scripts/studioctl set generatedRows 600
./scripts/studioctl generate --pattern circles --rows 600 \
  --output "$PWD/build/circles.csv"
./scripts/studioctl train --output "$PWD/build/circles.neuroevo"
./scripts/studioctl watch --interval 0.25
```

Run `./scripts/studioctl --help` for all commands or `./scripts/test-live-control` for an end-to-end packaged-app test. The DMG also includes `studioctl`, and the installed copy is under `NeuroevoStudio.app/Contents/Resources/`. The bridge uses a private per-user temporary directory and does not open a network port.

Full Xcode is not required for this build path; Swift 6 and macOS Command Line Tools are sufficient.

## Core ML export

The exporter uses an isolated optional Python environment because Apple distributes Core ML Tools as a Python package:

```bash
python3 -m venv .venv-coreml
source .venv-coreml/bin/activate
python -m pip install coremltools numpy

python tools/export_coreml.py \
  --model winner.neuroevo \
  --output winner.mlmodel

swiftc -parse-as-library tools/verify_coreml.swift \
  -framework CoreML -o build/verify_coreml
./build/verify_coreml winner.mlmodel 0,1
```

Normalization is baked into the first dense layer; regression target de-normalization is baked into the final layer. The native loader requests `.cpuAndNeuralEngine`. This makes compatible operations eligible for the Neural Engine but does not prove that every operation ran there—the Core ML runtime owns placement.

## Benchmark

```bash
./build/neuroevo_benchmark
./build-scalar/neuroevo_benchmark
./build/neuroevo_data_benchmark
./build/neuroevo_training_benchmark --help
```

The inference benchmark fixes its random seed and runs a `32x64x64x4` network. The
training benchmark defaults to the maximum configured topology of four 128-unit
hidden layers and measures both sequential and parallel partition evaluation. See
`docs/benchmarks/m5-pro-neuroevo-backends.md` and
`docs/benchmarks/m5-pro-maximum-training.md` for captured results.

## Limits and next extensions

- Numeric tabular data and dense feed-forward networks only
- No categorical/text/image preprocessing
- No gradient fine-tuning after evolution
- No NEAT speciation or innovation-number crossover
- Core ML export currently uses the maintained-but-feature-frozen neural-network model representation; ML Program export is a future extension

The `Dataset`, `Genome`, `Network`, evaluator, and evolution engine are separate so these can be extended without rewriting the CLI.

## Ownership and license

Neuroevo Studio and the Neuroevo C++ engine are original Neuralbytesfl project
code. Copyright © 2026 Neuralbytesfl.

The project is open-source software licensed under the [MIT License](LICENSE).
The license permits use, copying, modification, distribution, sublicensing, and
commercial sale provided that the copyright and permission notice remain included.
The software is provided without warranty, as described in the license.

Project home: [github.com/Neuralbytesfl/neural-network-experiments](https://github.com/Neuralbytesfl/neural-network-experiments)
