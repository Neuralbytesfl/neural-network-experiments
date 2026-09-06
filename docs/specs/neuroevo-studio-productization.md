# Spec: Neuroevo Studio Productization

## Goal
Turn Neuroevo Studio into a clean, educational, real-world macOS workflow where a person can generate or import data, inspect and clean it, evolve a neural network, use the resulting model, and understand every stage through embedded help.

## Current State
The application now trains classification and regression models from numeric CSV, generates and cleans data, visualizes evolution, predicts through the C++ model, exports Core ML, embeds a learning manual, supports live CLI control, and builds an ad-hoc-signed `.app` and DMG. The current quality audit found important gaps before it should be presented as a serious modeling workbench: no continuous integration, limited edge-case coverage, no naive-baseline comparison, insufficient metrics for imbalanced classification and regression, a non-stratified split, preprocessing that can leak information when used before splitting, no persistent experiment/model report, and no Swift unit or UI tests.

## Desired State
- Guided workflow: Welcome → Create/Import Data → Prepare Data → Train → Use Model → Learn.
- Built-in synthetic data generation for several classification and regression patterns.
- Reproducible seeds, noise controls, custom coefficients, CSV preview, and visualization.
- Dataset profiling and conservative cleaning for missing values, duplicates, malformed rows, and clipping.
- Clear separation between educational explanations and advanced controls.
- Embedded searchable manual covering data, cleaning, leakage, evolution, metrics, deployment, and limitations.
- Branded native icon at all required macOS icon sizes.
- Reproducible developer build, Release `.app`, DMG, signing, and verification scripts.
- A distributable unsigned/ad-hoc DMG today and documented Developer ID/notarization hooks for later.
- Two connected modes: a guided learning lab and a rigorous local workbench using the same engine and evidence.
- Every completed run compared with a deterministic naive baseline and accompanied by task-appropriate metrics.
- Reproducible experiment records that explain data, split, settings, limitations, and model fitness for use.
- Leakage-resistant fitted preprocessing, stratified classification splits, and explicit time-ordered split support.
- Automated scalar and Apple builds, sanitizers, Swift builds, accessibility audits, and release smoke tests.
- Honest failure guidance for small, imbalanced, constant, malformed, corrupt, and out-of-distribution inputs.
- Optional modern AI assistance only when it is grounded in local experiment evidence, clearly attributed, and never required to train or understand a model.

## Constraints
- Retain the existing C++ engine, CLI, model format, tests, and Core ML workflow.
- Numeric tabular CSV remains the initial production data boundary.
- Generated data must be clearly labeled synthetic and must never be represented as evidence about the real world.
- Cleaning defaults must be conservative and preserve the original input file.
- Full Xcode and paid Apple Developer credentials are not installed/available.
- DMG creation must work with built-in macOS command-line tools.
- No secrets or signing identities may be committed.

## Assumptions
- Common starter patterns (linear, polynomial, sine, XOR, circles, clusters, spiral) cover the educational use case.
- Real users benefit more from a visible staged workflow than one dense control screen.
- Advanced custom expression parsing can be added after the preset generator is stable.
- Ad-hoc signing is sufficient for local testing; public distribution requires Developer ID and notarization.

## Plan
1. Preserve the completed generation, cleaning, guided workflow, packaging, licensing, and CLI-control foundation.
2. Add a trust report that compares the evolved network with a train-derived naive baseline and reports balanced accuracy for classification or MAE/R-squared for regression.
3. Harden configuration and model/data validation against non-finite values, impossible dimensions, and unsafe resource requests.
4. Add automated Linux scalar and macOS Accelerate/Swift CI, plus a documented edge-case matrix.
5. Make splitting and preprocessing leakage-resistant, including stratification and time-aware choices.
6. Add experiment bundles, model cards, prediction export, and dataset schema/contracts.
7. Build the curriculum around executable experiments: prediction vs explanation, leakage, imbalance, overfitting, baselines, reproducibility, and model limits.
8. Add Swift unit/UI and accessibility coverage, clean-machine release testing, Developer ID signing, and notarization.

## Files Changed
- `include/neuroevo/data_tools.hpp`, `src/data_tools.cpp`: reusable generation, profiling, and cleaning API.
- `native/CNeuroevo/include/CNeuroevo.h`, `src/c_api.cpp`: stable C bridge for data operations.
- `tests/test_neuroevo.cpp`, `tools/benchmark_data.cpp`, `CMakeLists.txt`: deterministic coverage and throughput benchmark.
- `native/NeuroevoStudio/*.swift`: workflow shell and dedicated Welcome, Create, Prepare, Train, Use, and Learn views.
- `native/NeuroevoStudio/Assets/AppIcon-1024.png`, `Info.plist`: original icon and bundle metadata.
- `Package.swift`: native source/resource configuration.
- `scripts/build-iconset`, `build-macos-app`, `build-dmg`, `verify-release`, `dev-run`: development and distribution toolchain.
- `README.md`, `docs/manual`, `docs/runbooks`, `docs/blueprints`, `docs/benchmarks`: product, architecture, operations, and performance documentation.
- `datasets/tasks/neuroevo-studio-productization.md`: reusable task record.

## Commands Used
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
swift build -c release
./scripts/build-macos-app
./scripts/build-dmg --dry-run
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.dmg
```

## Verification
- [x] All seven generated CSV patterns parse through the production dataset loader.
- [x] Same-seed generation is byte-for-byte reproducible.
- [x] Cleaning preserves the source path and reports imputation, removal, drop, and clipping counts.
- [x] C++, C bridge, Swift debug, and Swift release builds pass without warnings.
- [x] AddressSanitizer and UndefinedBehaviorSanitizer test build passes.
- [x] App icon renders in the app and exists in the packaged bundle.
- [x] Welcome, Create, Prepare, Train, and Learn pages were visually and accessibility inspected.
- [x] DMG mounts, contains the `.app` and Applications link, and passes checksum, ARM64, signature, icon, and property-list checks.
- [x] Developer script help/dry-run and direct workflow-page launch are available.
- [ ] Developer ID signing and Apple notarization require the user's Apple credentials.
- [x] CI passes on a portable scalar build, sanitizers, and an Apple Accelerate/Swift build.
- [x] Classification reports accuracy, balanced accuracy, and majority-class baseline.
- [x] Regression reports normalized MSE, MAE, R-squared, and train-mean baseline.
- [x] Classification splitting is deterministic, proportional, and preserves sufficiently represented classes across all three partitions.
- [ ] Edge cases in `docs/testing/quality-matrix.md` have an automated or explicitly manual owner.
- [ ] Preprocessing is fitted using training data only for every production workflow.
- [ ] Main workflows pass keyboard, VoiceOver, contrast, larger-text, and reduce-motion checks.
- [x] Packaged-app CLI integration covers generation, training, trust metrics, prediction, null serialization, and navigation for classification and regression.

## Benchmark
Measured generation, profiling, and cleaning on 100,000 rows. Results are recorded in `docs/benchmarks/neuroevo-data-pipeline.md`. New UI callbacks remain outside the per-row training hot path.

## Rollback
Revert the productization commit. Local pre-change backups are stored under `.backups/2026-09-06/productize-studio/` and ignored by Git.

## Reuse Points
- Synthetic pattern library
- CSV profiler/cleaner
- Workflow navigation shell
- Embedded manual model
- App icon pipeline
- DMG/sign/verify scripts
- Task-independent baseline/evaluation report
- Machine-readable experiment evidence for reports and future grounded assistance
- Cross-platform CI and edge-case test matrix
