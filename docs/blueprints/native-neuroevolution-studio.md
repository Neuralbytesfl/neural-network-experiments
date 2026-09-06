# Blueprint: Native Neuroevolution Studio

## Purpose
Make the evolutionary learning process inspectable and controllable through a native macOS interface without duplicating the C++ learning implementation.

## Components
- `StudioModel`: main-actor Swift observable state.
- `ContentView`: SwiftUI dashboard and configuration sidebar.
- Swift Charts: live training and validation series.
- `TrainingWorker`: background bridge worker and callback owner.
- `CNeuroevo`: stable C ABI around C++ dataset, evolution, cancellation, and prediction APIs.
- `EvolutionEngine`: existing C++ implementation with generation callbacks.
- `export_coreml.py`: optional Core ML model exporter.
- `verify_coreml.swift`: native Core ML inference verifier.
- `build-macos-app`: SwiftPM build and `.app` packaging workflow.

## Data Flow
```text
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
```

## Control Flow
1. Swift selects and inspects a CSV through the C bridge.
2. A background worker owns the opaque training session.
3. C++ evaluates a generation and invokes one callback.
4. Swift copies the callback payload before the C strings expire.
5. A main-actor task updates observable UI state.
6. Stop sets an atomic flag; C++ exits after the current generation and saves its hall-of-fame model.
7. Prediction calls the saved-model C API.
8. Optional Core ML export bakes normalization into dense weights and biases.

## Configuration
Data/output paths, task, header, target columns, population, generations, elites, threads, seed, hidden width, patience, score target, mutation parameters, and complexity penalty are editable.

## Security
- No network access is performed by the app.
- File access is limited to paths selected or supplied by the user.
- The optional exporter launches a known bundled script with argument arrays, not a shell command string.
- No secrets are stored.
- The local app is ad-hoc signed and initially unsandboxed.

## Failure Modes
- Invalid CSV or evolution configuration is returned through a bounded error buffer.
- Missing Core ML Python dependencies produces an actionable message.
- Cancellation waits for the active generation to complete.
- SwiftUI can outlive a callback only because callback values are copied immediately.
- Core ML may choose CPU execution for small graphs even when Neural Engine use is allowed.

## Recovery
Training failures leave prior saved models untouched unless a new run reaches its save stage. Re-run with a known seed and configuration. Rebuild the `.app` from source using the packaging script.

## Verification
- C bridge callback, cancellation, inspection, and prediction tests.
- Debug and Release Swift builds.
- Ad-hoc signature and property-list checks.
- Visual inspection of initial and populated dashboards.
- Four-case XOR equivalence through native Core ML.

## Benchmark
Only one callback is emitted per generation, keeping UI synchronization outside the per-sample and per-candidate hot loops. Existing backend performance remains the relevant compute benchmark.

## Reuse Points
- The C ABI can support AppKit, Qt, web, or Python front ends.
- Metric cards, topology view, and chart panel are independent SwiftUI components.
- The Core ML exporter consumes the stable versioned model format.

