# Task Record: Neuroevo Studio Productization

## Date
2026-09-06

## Task Type
application, machine-learning, data-pipeline, packaging, documentation

## Original Request
Make the native application easy enough to generate and clean data, build and use a neural network, teach the concepts through embedded help, look like a real macOS product, produce a DMG, and remain convenient to develop.

## Problem
The existing app exposed training in one dense dashboard. It had no built-in data generation or cleaning, embedded manual, branded icon, or DMG/notarization workflow.

## Environment
Confirmed: Apple M5 Pro MacBook Pro, ARM64, macOS 26.6.2, Swift 6, C++20, Apple Accelerate.

Assumed: Initial production data remains numeric tabular CSV.

Unknown: Final Apple Developer identity and notarization keychain profile.

## Inputs
- Existing C++ neuroevolution engine and C bridge
- Existing SwiftUI dashboard and Core ML exporter
- User requirements in the active task

## Actions Taken
1. Added deterministic regression/classification data patterns.
2. Added profiling and non-destructive cleaning with audit counts.
3. Reorganized the native app into a staged workflow.
4. Added a searchable embedded manual and responsible-use guidance.
5. Added an original icon and `.icns` generation.
6. Added app, DMG, Developer ID/notarization, verification, and development scripts.
7. Added tests, benchmarks, user documentation, and release runbook.

## Files Changed
See `docs/specs/neuroevo-studio-productization.md` for the categorized file list.

## Commands Used
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
swift build
./build/neuroevo_data_benchmark
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.dmg
```

## Output
An ad-hoc-signed ARM64 macOS application and verified DMG, plus reusable source modules and packaging scripts.

## Verification
Confirmed: C++ tests passed; Swift debug and release builds passed; generated patterns loaded through the production loader; DMG checksum, mounted structure, bundle property list, icon, ARM64 binary, and signature passed automated checks; major UI pages passed visual and accessibility inspection.

Not Verified: Developer ID signing and Apple notarization, because credentials were not available.

## Benchmark
100,000-row release run: generation 1,565,720 rows/s; profiling 935,472 rows/s; cleaning 734,676 rows/s. One run only; see the benchmark record for caveats.

## Rollback
Revert the productization commit. Pre-change files are also available under the ignored `.backups/2026-09-06/productize-studio/` directory.

## Reusable Pattern
Keep data generation/cleaning in a UI-independent core, expose a stable C ABI, keep SwiftUI on the main actor, and make packaging/signing configuration external to source.

## Fine-Tuning Usefulness
High

## Notes
Synthetic data is explicitly presented as a learning and validation tool, not proof of real-world fitness. Core ML Neural Engine eligibility is not represented as guaranteed Neural Engine placement.
