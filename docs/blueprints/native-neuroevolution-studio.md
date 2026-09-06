# Blueprint: Native Neuroevolution Studio

## Purpose
Make the evolutionary learning process inspectable and controllable through a native macOS interface without duplicating the C++ learning implementation. The product has two views of one evidence pipeline: Guided Lab teaches why each decision matters; Workbench exposes the same decision with professional controls and reports.

## Components
- `StudioModel`: main-actor Swift observable state.
- `ContentView`: workflow navigation shell and shared visual components.
- `DataLabView` / `PrepareDataView`: generation, import, profiling, cleaning, and preview.
- `TrainingView` / `UseModelView`: evolution dashboard, native prediction, and Core ML export.
- `LearnView`: searchable embedded educational manual.
- `StudioControl`: private local request/reply bridge over the same observable GUI state.
- Swift Charts: live training and validation series.
- `TrainingWorker`: background bridge worker and callback owner.
- `CNeuroevo`: stable C ABI around C++ data tools, evolution, cancellation, and prediction APIs.
- `data_tools`: deterministic synthetic generator plus auditable CSV profiler/cleaner.
- `EvolutionEngine`: existing C++ implementation with generation callbacks.
- `EvaluationReport`: task-aware comparison against a train-derived naive baseline, with balanced accuracy or regression diagnostics.
- Experiment evidence (planned): immutable data fingerprint, split, preprocessing recipe, settings, seed, metrics, limitations, and artifact links.
- `export_coreml.py`: optional Core ML model exporter.
- `verify_coreml.swift`: native Core ML inference verifier.
- Release scripts: iconset, SwiftPM app bundle, DMG, Developer ID/notarization hooks, and verification.

## Data Flow
```text
Preset/import -> C++ generate/profile/clean -> prepared CSV
                                              |
                                              v
CSV picker -> Swift settings -> C ABI -> C++ Dataset/EvolutionEngine
                                      |
                                      v
Swift main actor <- copied snapshots <- generation callback
       |
       +-> Chart / cards / topology / logs
       |
       +-> saved .neuroevo -> Core ML exporter -> .mlmodel
                                                    |
                                                    v
                                    Core ML CPU + Neural Engine allowed

studioctl -> private per-user JSON queue -> StudioModel -> SwiftUI updates
                                      \-> structured state reply
```

## Control Flow
1. Swift generates or imports a CSV, then profiles it through the C bridge.
2. Optional cleaning writes a separate output and returns exact audit counts.
3. A background worker owns the opaque training session.
4. C++ evaluates a generation and invokes one callback.
5. Swift copies callback values and updates main-actor UI state.
6. Stop sets an atomic flag; C++ exits after the current generation and saves its hall-of-fame model.
7. Prediction calls the saved-model C API.
8. Optional Core ML export bakes normalization into dense weights and biases.
9. The winner is judged against a baseline before the UI describes it as useful.
10. Guided Lab turns the run evidence into explanations; Workbench turns it into an exportable experiment report.

## Configuration
Pattern, size, range, noise, generator seed, cleaning recipe, data/output paths, task, header, target columns, population, generations, elites, threads, model seed, hidden width, patience, score target, mutation parameters, and complexity penalty are editable.

## Security
- No network access is performed by the app.
- CLI control uses a mode-0700 per-user temporary directory and no network listener.
- Only whitelisted commands and setting names are accepted.
- File access is limited to paths selected or supplied by the user.
- The optional exporter launches a known bundled script with argument arrays, not a shell command string.
- No secrets are stored.
- Source datasets are never overwritten by cleaning.
- The local app is ad-hoc signed and initially unsandboxed.

## Failure Modes
- Invalid CSV or evolution configuration is returned through a bounded error buffer.
- Malformed rows, missing targets, and unwritable destinations return actionable errors.
- Missing Core ML Python dependencies produces an actionable message.
- Cancellation waits for the active generation to complete.
- SwiftUI can outlive a callback only because callback values are copied immediately.
- Core ML may choose CPU execution for small graphs even when Neural Engine use is allowed.
- A high accuracy can still be worse than a majority baseline on imbalanced data.
- Cleaning statistics computed before splitting can leak evaluation information; this remains a release-blocking limitation until fitted transforms are integrated.
- Classification uses a deterministic proportional split. Time-dependent data still needs a chronological policy, and extremely rare classes still require an explicit warning.
- Optional generated explanations can be wrong; future assistance must cite only measured local evidence and link back to deterministic documentation.

## Recovery
Training failures leave prior saved models untouched unless a new run reaches its save stage. Re-run with a known seed and configuration. Rebuild the `.app` from source using the packaging script.

## Verification
- C bridge callback, cancellation, inspection, and prediction tests.
- Debug and Release Swift builds.
- Ad-hoc signature and property-list checks.
- Visual and accessibility inspection of Welcome, Create, Prepare, Train, and Learn pages.
- Mounted DMG integrity, ARM64 binary, icon, property list, signature, and Applications link.
- Four-case XOR equivalence through native Core ML.
- Baseline and task-specific metric tests, including imbalanced classification and constant-target regression.
- CI on portable scalar C++ and native Apple Accelerate/Swift builds.
- Quality ownership and release gates in `docs/testing/quality-matrix.md`.

## Benchmark
Only one callback is emitted per generation, keeping UI synchronization outside the per-sample and per-candidate hot loops. The 100,000-row generator/profiler/cleaner benchmark is recorded in `docs/benchmarks/neuroevo-data-pipeline.md`.

## Reuse Points
- The C ABI can support AppKit, Qt, web, or Python front ends.
- The deterministic data module can be used without the GUI.
- Metric cards, topology view, and chart panel are independent SwiftUI components.
- The Core ML exporter consumes the stable versioned model format.
- Release scripts accept paths and signing configuration without committed credentials.
