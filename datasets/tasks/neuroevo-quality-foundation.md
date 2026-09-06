# Task Record: Neuroevo Studio Quality Foundation

## Date
2026-09-06

## Task Type
product quality, evaluation, testing, architecture, education

## Original Request
Refine Neuroevo Studio beyond code generation into a top-quality, edge-tested, real-world-useful, modern AI product whose educational role remains central.

## Problem
The app could generate/clean numeric data, evolve a model, visualize progress, predict, export, and package successfully, but a completed run did not establish whether the learned model added value over a trivial rule. Classification accuracy could conceal imbalance, regression lacked interpretable companion metrics, classification splitting was not proportional, automated CI was absent, and edge/release ownership was not explicit.

## Environment

Confirmed:
- macOS on Apple Silicon with AppleClang 21, Swift 6, CMake, Apple Accelerate, and the native SwiftUI app.
- Public GitHub repository under Neuralbytesfl.

Assumed:
- Numeric tabular classification and regression remain the current supported production boundary.

Unknown:
- Developer ID/notarization credentials and behavior on every supported clean macOS version.

## Inputs
- Existing C++20 engine, C ABI, SwiftUI app, tests, package scripts, manual, and benchmark records.
- Apple machine-learning and accessibility design guidance.
- NIST AI RMF/Playbook guidance for test/evaluation evidence and failure analysis.
- GitHub-hosted runner and official action documentation.

## Actions Taken
1. Audited engine, dataset flow, C boundary, SwiftUI workflow, tests, release scripts, and documentation.
2. Updated the product spec and blueprint with dual Guided Lab/Workbench direction and explicit release gates.
3. Added a release-quality edge-case matrix.
4. Implemented task-aware evaluation against a train-derived naive baseline.
5. Added classification balanced accuracy and regression normalized MAE/R-squared.
6. Added deterministic proportional classification splitting.
7. Hardened evolution settings, model parameters/dimensions, resource bounds, and prediction inputs.
8. Added Linux scalar, sanitizer, and macOS Accelerate/Swift CI jobs.
9. Added Trust Report UI, explicit “did not beat baseline” status, manual lessons, CLI output, and live-control metrics.
10. Extended packaged-app integration to train and verify both classification and regression.
11. Found and fixed non-finite JSON serialization during the real packaged-app test.

## Files Changed
- Core evaluation/config/model: `include/neuroevo/neuroevo.hpp`, `src/neuroevo.cpp`, `src/main.cpp`.
- Native boundary: `native/CNeuroevo/include/CNeuroevo.h`, `src/c_api.cpp`.
- UI and education: `StudioModel.swift`, `StudioControl.swift`, `TrainingView.swift`, `LearnView.swift`, `Info.plist`.
- Verification: `tests/test_neuroevo.cpp`, `scripts/test-live-control`, `.github/workflows/ci.yml`.
- Product evidence: README, spec, blueprint, quality matrix, and NaN error record.

## Commands Used
```bash
cmake -S . -B build-quality -DCMAKE_BUILD_TYPE=Debug -DNEUROEVO_ENABLE_ACCELERATE=ON
cmake --build build-quality --parallel
ctest --test-dir build-quality --output-on-failure
cmake -S . -B build-quality-scalar -DCMAKE_BUILD_TYPE=Release -DNEUROEVO_ENABLE_ACCELERATE=OFF
cmake --build build-quality-scalar --parallel
ctest --test-dir build-quality-scalar --output-on-failure
cmake -S . -B build-quality-sanitize -DCMAKE_BUILD_TYPE=Debug -DNEUROEVO_ENABLE_ACCELERATE=OFF -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-quality-sanitize --parallel --target neuroevo_tests
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-quality-sanitize --output-on-failure
swift build
./scripts/build-macos-app --output dist
./scripts/test-live-control
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.dmg
```

## Output
- Version 0.3.0 surfaces measured evidence before presenting a trained model as useful.
- CLI and GUI expose the same task-aware report.
- Classification and regression end-to-end workflows are repeatable through the live control bridge.
- Automated CI configuration covers portable and Apple-native builds.

## Verification
Confirmed:
- Accelerate debug C++ build and tests passed.
- Scalar release C++ build and tests passed.
- AddressSanitizer/UndefinedBehaviorSanitizer tests passed locally with Apple leak detection disabled due the recorded platform limitation.
- Swift debug build passed.
- Swift release build passed.
- Packaged release app completed classification and regression generate/train/predict workflows.
- Trust Report layout was visually inspected in the running native app.
- Version 0.3.0 DMG checksum, property list, Apple Silicon binary, ad-hoc signature, and bundle layout passed release verification.

Not Verified:
- Remote GitHub Actions status until the commit is pushed.
- VoiceOver and automated Swift UI tests.
- Developer ID signing/notarization and clean-machine installation.

## Benchmark
This phase does not alter the per-generation training hot path. Trust evaluation runs once for the selected winner; no before/after training-speed claim is made.

## Rollback
Revert the quality-foundation commit. Versioned models remain format-compatible because the saved model format did not change. The C result structure gained metrics and therefore the native app and C bridge must be rebuilt together when rolling between versions.

## Reusable Pattern
- Compare learned systems with the simplest credible baseline before calling them useful.
- Represent non-applicable metrics as optional/null at external serialization boundaries.
- Teach from the exact evidence the production workflow measures.

## Fine-Tuning Usefulness
High

## Notes
The next release-blocking work is train-fitted preprocessing, chronological split support, quoted-field CSV, experiment/model reports, Swift test targets, accessibility audits, and notarization.
