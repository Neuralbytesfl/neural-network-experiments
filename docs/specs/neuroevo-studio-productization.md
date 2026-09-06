# Spec: Neuroevo Studio Productization

## Goal
Turn Neuroevo Studio into a clean, educational, real-world macOS workflow where a person can generate or import data, inspect and clean it, evolve a neural network, use the resulting model, and understand every stage through embedded help.

## Current State
The application trains classification and regression models from numeric CSV, visualizes evolution, predicts through the C++ model, exports Core ML, and builds as an ad-hoc-signed `.app`. Data creation and cleaning require external tools; configuration is concentrated in one view; help is limited to repository documentation; there is no branded icon or DMG pipeline.

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
1. Add reusable C++ data generation, profiling, and cleaning modules plus C bridge APIs.
2. Add deterministic tests for every data pattern and cleaning operation.
3. Refactor SwiftUI into a workflow sidebar with dedicated Data Lab, Prepare, Train, Use, and Learn screens.
4. Add previews, charts, recommendations, contextual help, and model-use examples.
5. Integrate an original generated icon and produce a valid `.icns` resource.
6. Add developer, app, DMG, signing, and verification scripts with help/dry-run modes.
7. Build, run sanitizers, generate sample artifacts, inspect the real GUI, and record results.

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
