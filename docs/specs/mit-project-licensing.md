# Spec: MIT Project Licensing

## Goal
Clearly identify Neuroevo Studio as a Neuralbytesfl project and publish it under the
OSI-approved MIT License in the repository and packaged macOS artifacts.

## Current State
The public GitHub repository has no root license file or explicit copyright-holder
statement. The application and DMG do not embed a copy of a software license.

## Desired State
- GitHub detects a canonical root MIT license.
- The copyright notice identifies Neuralbytesfl and the year 2026.
- The README visibly states ownership, license, permissions, and warranty status.
- CMake and macOS bundle metadata identify the project and copyright holder.
- Every generated app and DMG contains the license text.
- Release verification fails if the embedded license is missing.

## Constraints
- Preserve the canonical MIT permission and warranty text.
- Do not imply trademark rights or patent grants beyond the MIT terms.
- Do not change source behavior, model formats, or user data.

## Assumptions
- `Neuralbytesfl` is the requested public copyright-holder name, based on the GitHub
  repository owner and the user's request to identify the program as theirs.
- The copyright year is 2026, when this repository was created and published.

## Plan
1. Add a root `LICENSE` containing canonical MIT text.
2. Add a visible ownership and license section to the README.
3. Add project homepage/description and macOS copyright metadata.
4. Embed the license into `.app` and `.dmg` output.
5. Extend release checks, build and verify both artifacts, then inspect GitHub's API.
6. Create a reusable task record, commit, and publish to `main`.

## Files Changed
- `LICENSE`: canonical license and copyright notice.
- `README.md`: ownership and licensing explanation.
- `CMakeLists.txt`: project description and homepage metadata.
- `native/NeuroevoStudio/Info.plist`: bundle copyright metadata.
- `scripts/build-macos-app`: embed license in application resources.
- `scripts/build-dmg`: expose license at disk-image root.
- `scripts/verify-release`: verify license presence and copyright notice.
- `docs/specs/mit-project-licensing.md`: this specification.
- `datasets/tasks/mit-project-licensing.md`: reconstructable task record.

## Commands Used
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./scripts/build-macos-app
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.app
./scripts/verify-release dist/NeuroevoStudio.dmg
```

## Verification
- [x] Canonical license text present
- [x] Release build and tests pass
- [x] Application contains and verifies `LICENSE.txt`
- [x] DMG contains and verifies `LICENSE.txt`
- [x] GitHub reports SPDX identifier `MIT`

## Benchmark
Not applicable; licensing and package metadata do not affect runtime behavior.

## Rollback
Revert the licensing commit. Note that previously distributed MIT copies retain the
rights already granted under that copy of the license.

## Reuse Points
- Release scripts guarantee that recipients receive the governing license.
- Bundle metadata and README attribution can be reused by later distribution flows.
