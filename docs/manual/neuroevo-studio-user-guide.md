# Neuroevo Studio User Guide

## Purpose
Neuroevo Studio helps a person create or import numeric tabular data, inspect and clean it, evolve a dense neural network, test predictions, and export the winning model to Core ML. The same essential guidance is available inside the app under **Learn**.

## Quick Start
1. Open **Create Data** and generate Circles with 1,000 rows, seed 42, and modest noise—or import a numeric CSV.
2. Open **Prepare Data**, confirm the header and task type, review the profile, and create a cleaned copy if needed.
3. Open **Train Model**, choose a `.neuroevo` destination, and start evolution.
4. Compare training, validation, and held-out test performance. Prefer a compact network that generalizes.
5. Open **Use Model**, enter comma-separated features in training-column order, and run a prediction.
6. Optionally export `.mlmodel` for an Apple application.

## CSV Contract
- One example per row.
- Numeric feature columns first.
- Target column or columns last.
- Classification labels must be finite numeric values.
- The header option must match the file.
- Live prediction must preserve feature count, order, units, and preprocessing.

## Synthetic Patterns
Linear, polynomial, and sine are regression datasets. XOR, circles, clusters, and spiral are classification datasets. The seed reproduces random sampling. Noise makes a pattern less exact. Synthetic data is useful for learning, debugging, and benchmarking; it is not evidence of real-world performance.

## Cleaning
The cleaner always writes a new file. It can fill missing feature values with column means, drop rows with missing targets, remove exact duplicates, drop malformed rows, and optionally clip extreme feature values. It never invents a missing target.

For production work, split first. Learn imputation and clipping values only from the training partition, freeze them, and apply the same transformations to validation, test, and live inputs. Otherwise preprocessing can leak information.

## Evolution
A population contains candidate networks with weights and topologies. Strong candidates survive or become parents. Mutations alter weights, activations, hidden-layer sizes, and depth. Fitness combines predictive performance with a small complexity penalty. Validation selects the saved winner; test data provides the final check.

## Reading Results
High training performance with lower validation performance can indicate overfitting. One score is rarely enough for production: consider confusion matrices, precision/recall, calibration, subgroup failures, latency, memory, and domain-specific error costs.

## Apple Silicon
Evolution uses native C++ threads and Apple Accelerate CPU kernels. Apple does not expose Neural Engine instructions as ordinary C++ intrinsics. Exported Core ML models request CPU and Neural Engine compute, but Core ML decides actual graph placement. Benchmark on the target device.

## Responsible Use
Predictions are fallible estimates. Do not use this experimental tool as the sole basis for medical, legal, financial, safety-critical, or other high-impact decisions. Validate representative data, bias, privacy, failure handling, and monitoring for the actual use case.

## Troubleshooting
- **Dataset rejected:** verify numeric cells, header setting, equal row width, target position, and enough rows for all splits.
- **Training stalls:** inspect labels and preview, reduce noise, increase the search budget gradually, and compare fixed seeds.
- **Export fails:** install the isolated Core ML Tools environment described in `docs/runbooks/coreml-export.md`; native prediction does not require it.
