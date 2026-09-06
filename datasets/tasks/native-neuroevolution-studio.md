# Task Record: Native Neuroevolution Studio

## Date
2026-09-06

## Task Type
GUI, C++, SwiftUI, machine-learning, Core ML

## Original Request
Extend the neuroevolution project with the proposed Apple deployment changes and a native GUI showing visual evolution, data, and metrics.

## Problem
The C++ engine was headless and synchronous, with no stable GUI bridge, cancellation, live generation feed, native macOS interface, or tested Core ML deployment artifact.

## Environment
Confirmed:
- Apple M5 Pro and macOS 26.6.2
- Swift 6.3.3
- Apple Clang 21
- Command Line Tools present
- Full Xcode absent
- Core ML Tools initially absent; 9.0 installed into an ignored isolated environment for verification

## Inputs
- Existing C++ engine and model format
- Apple SwiftUI, Swift Charts, Core ML, and Accelerate APIs
- User-requested live data and metric visibility

## Actions Taken
1. Added per-generation callback and cooperative cancellation to C++.
2. Added a bounded, exception-safe C ABI for dataset inspection, training, progress, stop, and prediction.
3. Added a SwiftUI/Charts dashboard and background training worker.
4. Added deterministic command-line preload arguments for demos and UI verification.
5. Added an app-bundle packager with help and dry-run modes.
6. Added Core ML export with baked normalization and a native inference verifier.
7. Built, tested, signed, launched, and visually inspected the application.

## Files Changed
See `docs/specs/native-neuroevolution-studio.md`.

## Commands Used
```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
swift build -c debug
./scripts/build-macos-app --dry-run
./scripts/build-macos-app
codesign --verify --deep --strict dist/NeuroevoStudio.app
python tools/export_coreml.py --model build/coreml-xor.neuroevo --output build/coreml-xor.mlmodel
swiftc -parse-as-library tools/verify_coreml.swift -framework CoreML -o build/verify_coreml
```

## Output
An ad-hoc-signed native arm64 macOS application with live evolutionary visualization and optional Neural-Engine-eligible Core ML deployment.

## Verification
Confirmed:
- C++ bridge and existing engine tests passed.
- Swift debug and release builds passed without warnings.
- `.app` property list and signature checks passed.
- Initial GUI state and populated live regression dashboard were visually inspected.
- Live chart, topology, dataset summary, metrics, evaluation count, log, and prediction were populated.
- Core ML exporter succeeded with Core ML Tools 9.0.
- Native Swift Core ML inference with `.cpuAndNeuralEngine` returned correct classes for all four XOR inputs.
- Core ML regression output matched the C++ raw prediction within expected floating-point precision.

Not Verified:
- Actual per-operation Neural Engine placement; Core ML owns scheduling.
- Distribution signing/notarization with an Apple Developer identity.

## Benchmark
The GUI receives one callback per generation, outside the candidate evaluation hot loop. Existing M5 backend benchmark remains unchanged.

## Rollback
Revert the implementation commit. Local pre-change backups exist in the ignored `.backups/2026-09-06/native-gui/` folder.

## Reusable Pattern
Use a narrow C ABI between a high-performance C++ core and SwiftUI, copy callback data synchronously, and marshal only immutable snapshots onto the main actor.

## Fine-Tuning Usefulness
High

## Notes
Neural Engine execution is requested through Core ML compute units and never represented as direct C++ NPU instructions.
