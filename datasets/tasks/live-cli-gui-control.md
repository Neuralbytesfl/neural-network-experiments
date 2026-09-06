# Task Record: Live CLI-to-GUI Control

## Date
2026-09-06

## Task Type
application, automation, testing, developer-tooling

## Original Request
Make CLI changes appear in the GUI in real time so the application can be controlled and tested without relying on slower GUI automation.

## Problem
Startup arguments could configure a new process, but no structured interface existed for a running app.

## Environment
Confirmed: packaged ARM64 Neuroevo Studio on Apple M5 Pro and macOS 26.6.2.

## Actions Taken
1. Moved workflow navigation into shared observable state.
2. Added a private local JSON request/reply processor.
3. Added whitelisted navigation, settings, data, training, and prediction commands.
4. Added structured status, watch, wait, dry-run, and error behavior.
5. Added a packaged CLI and end-to-end self-test.
6. Controlled the release app entirely from CLI and visually inspected reflected values.

## Files Changed
See `docs/specs/live-cli-gui-control.md`.

## Commands Used
```bash
./scripts/studioctl status
./scripts/test-live-control
./scripts/studioctl set generatedRows 777
```

## Output
A dependency-free local controller available from the repository, DMG root, and packaged app resources.

## Verification
Confirmed: a CLI-generated 240-row spiral dataset was profiled and trained; the model file was saved; prediction returned a class and probabilities; navigation ended on Learn; the GUI visibly showed Create Data, Spiral, 777 rows, 0.02 noise, seed 777, preview points, and CLI-control status.

Confirmed: invalid arbitrary setting names are rejected with a nonzero CLI result.

## Benchmark
Median round trip was 71.834 ms over 20 status commands; maximum was 99.270 ms.

## Rollback
Revert the live-control commit. Existing model and CSV formats are unchanged.

## Reusable Pattern
Expose a narrow, typed command surface over the same observable model used by a native GUI. Prefer structured state over brittle screen scraping for automation.

## Fine-Tuning Usefulness
High

## Notes
The bridge is local-user-only and network-free. It assumes one active app instance per user.
