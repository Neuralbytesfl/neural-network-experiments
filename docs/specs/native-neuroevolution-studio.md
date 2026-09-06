# Spec: Native Neuroevolution Studio

## Goal
Create a native macOS GUI that drives the existing C++ neuroevolution engine and makes dataset state, live evolutionary progress, network topology, metrics, logs, saved models, and predictions visible.

## Current State
The repository contains a verified C++20 CLI/library with CSV loading, model evolution, persistence, tests, and an Apple Accelerate backend. It has no live callback/cancellation API, C-compatible bridge, GUI, or Core ML export workflow. The host has Swift 6.3 and Command Line Tools but not full Xcode or `coremltools`.

## Desired State
- A SwiftUI `.app` built through Swift Package Manager and a repeatable packaging script.
- Dataset picker and configuration controls for classification/regression searches.
- Live generation chart, headline metrics, dataset split summary, topology display, logs, progress, start/stop, and result state.
- Saved-model prediction from a raw feature row.
- C ABI between Swift and the C++ engine with callback lifetime and cancellation defined.
- Optional Core ML export tooling that fails with actionable instructions when dependencies are missing.
- Core ML runtime configuration that permits CPU plus Neural Engine for exported compatible models.

## Constraints
- Preserve the CLI and public C++ API.
- SwiftUI and Swift Charts require macOS 13 or later; target macOS 14.
- Command Line Tools are installed, but Xcode is not currently selected/available.
- Apple exposes the Neural Engine through Core ML scheduling, not public C++ intrinsics.
- GUI updates must be marshalled to the Swift main actor.
- Evolution must remain off the UI thread.

## Assumptions
- A Swift Package executable can be packaged into a macOS `.app` without an Xcode project.
- The application will initially run unsandboxed so it can access user-selected local CSV/model paths.
- Core ML export may require a separately managed Python environment and will not be claimed verified until exercised.

## Plan
1. Add generation callbacks and cooperative cancellation to `EvolutionEngine`.
2. Add a narrow C API for inspection, training, cancellation, and prediction.
3. Add a Swift package containing the C++ core target and native SwiftUI executable target.
4. Build dashboard, charts, topology view, file dialogs, configuration, prediction, and logs.
5. Add a reproducible app-bundle script with `--help` and `--dry-run`.
6. Add optional ML Program export and Core ML inference path.
7. Build, run tests, launch/screenshot the GUI, and document verification and rollback.

## Files Changed
- `.gitignore`: ignore local app, Swift, Core ML environment, and backup artifacts.
- `Package.swift`: Swift/C++ package and Apple framework linkage.
- `CMakeLists.txt`: C bridge library and warning configuration.
- `include/neuroevo/neuroevo.hpp`: generation callback API.
- `src/neuroevo.cpp`: callback and cancellation handoff point.
- `src/c_api.cpp`: exception-safe C bridge.
- `native/CNeuroevo/include/CNeuroevo.h`: stable C ABI.
- `native/NeuroevoStudio/*.swift`: native application, state, charts, controls, and Core ML runtime.
- `native/NeuroevoStudio/Info.plist`: application metadata.
- `scripts/build-macos-app`: reproducible `.app` builder.
- `tools/export_coreml.py`: model converter.
- `tools/verify_coreml.swift`: native deployment verifier.
- `tests/test_neuroevo.cpp`: bridge, callback, cancellation, and prediction tests.
- `README.md`: GUI and Core ML instructions.
- `docs/blueprints/native-neuroevolution-studio.md`: system design.
- `docs/runbooks/coreml-export.md`: operational export workflow.
- `datasets/tasks/native-neuroevolution-studio.md`: task record.
- `datasets/errors/coremltools-datatypes-import.md`: corrected conversion failure.

## Commands Used
```bash
swift build -c release
./scripts/build-macos-app --dry-run
./scripts/build-macos-app
open dist/NeuroevoStudio.app
```

## Verification
- [x] Existing and new C++ tests pass.
- [x] Bridge tests cover callback, cooperative cancellation, dataset inspection, and prediction.
- [x] Swift package compiles in debug and release modes without warnings.
- [x] Packaged arm64 `.app` is ad-hoc signed and has a valid property list.
- [x] Initial and populated GUI states were visually inspected.
- [x] Live regression chart displayed 70 generations and a changed topology.
- [x] Core ML Tools 9.0 export succeeded in an isolated environment.
- [x] Native `.cpuAndNeuralEngine` inference produced correct XOR classes.
- [x] Classification probabilities and regression de-normalization matched the C++ model after Core ML export.
- [ ] Actual Neural Engine placement was not proven because Core ML controls scheduling.

## Benchmark
The bridge emits one callback per generation rather than per candidate or sample, so UI work remains outside the compute hot loop. No material change to the existing inference benchmark was expected or claimed.

## Rollback
Revert the implementation commit. Pre-change copies are retained under `.backups/2026-09-06/native-gui/` locally and ignored by Git.

## Reuse Points
- Stable C bridge for other front ends
- Observable Swift training controller
- Reusable metric/topology chart components
- Repeatable command-line `.app` packager
- Optional Core ML deployment adapter
