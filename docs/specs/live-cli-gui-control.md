# Spec: Live CLI-to-GUI Control

## Goal
Let developers and automation tools navigate and control a running Neuroevo Studio instance through a fast local CLI while every change appears immediately in the native GUI.

## Current State
The app accepts startup arguments such as `--data`, `--output`, and `--page`, but a running process cannot be queried or changed without GUI interaction.

## Desired State
- `studioctl` discovers the current user’s running app automatically.
- Commands navigate pages, update whitelisted settings, generate/profile/clean data, start/stop training, and predict.
- CLI receives structured JSON state and errors.
- GUI changes appear within 100–200 ms without stealing focus.
- A status indicator explains that local CLI control is available.
- The packaged app and development build use the same protocol.

## Constraints
- Local machine and current Unix user only; no network listener.
- No third-party runtime dependencies.
- Never execute arbitrary code or accept arbitrary model-property names.
- File-writing actions require explicit paths so automation never opens a file picker.
- Preserve normal GUI operation.

## Assumptions
- A file-queue under the user’s temporary directory is sufficiently real-time for development and testing.
- One active app instance per user is the normal workflow.
- JSON output is the most reusable interface for scripts and future agents.

## Plan
1. Move workflow selection into shared observable app state.
2. Add a main-actor local command processor with an atomic JSON request/reply protocol.
3. Add a dependency-free Python CLI with help, dry-run, timeout, and JSON output.
4. Add CLI commands for navigation, settings, data operations, training, stopping, prediction, and status.
5. Add protocol/unit tests and a self-test script.
6. Package the controller, drive the real app with it, and visually verify GUI reflection.
7. Document security, commands, verification, performance, and rollback.

## Files Changed
- `native/NeuroevoStudio/StudioControl.swift`: local command processor and structured state.
- `native/NeuroevoStudio/StudioModel.swift`, `ContentView.swift`: shared navigation state and control loop.
- `native/NeuroevoStudio/LearnView.swift`: embedded CLI automation lesson.
- `scripts/studioctl`: dependency-free JSON CLI.
- `scripts/test-live-control`: packaged-app end-to-end self-test.
- `scripts/build-macos-app`, `build-dmg`, `verify-release`: package and verify the controller.
- `README.md`, user guide, live-control runbook and benchmark: usage and operations documentation.
- `datasets/tasks/live-cli-gui-control.md`: reusable implementation record.

## Commands Used
```bash
./scripts/studioctl --help
./scripts/studioctl status
./scripts/studioctl page create
./scripts/studioctl set generatedRows 600
./scripts/studioctl generate --pattern spiral --output /tmp/studio-data.csv
./scripts/studioctl train --output /tmp/studio-model.neuroevo
./scripts/studioctl watch
```

## Verification
- [x] Swift debug build passes without warnings.
- [x] Existing C++ test suite remains green.
- [x] Invalid setting names return nonzero with a useful error.
- [x] CLI state matches visible page, pattern, rows, noise, seed, preview, and status.
- [x] Generation, profiling, training, waiting, and prediction work against the packaged app.
- [x] Self-test validates generated data/model files and final state.
- [x] Request latency measured over 20 independent CLI processes.

## Benchmark
Twenty status requests: 64.930 ms mean, 71.834 ms median, 75.875 ms p95, and 99.270 ms maximum. See `docs/benchmarks/live-cli-control.md`.

## Rollback
Revert the live-control commit. The app continues to work without the CLI protocol.

## Reuse Points
- Local JSON request/reply queue
- Whitelisted observable-state mutation
- Headless test and automation commands
- Structured model-training status output
