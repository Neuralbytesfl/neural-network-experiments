# Runbook: Live CLI Control

## Purpose
Control and observe a running Neuroevo Studio GUI from a terminal or automation agent without slow GUI clicking.

## Prerequisites
- Start the development or packaged app.
- Use the same local Unix account for the app and CLI.
- Python 3 standard library; no packages are required.

## Steps
```bash
./scripts/studioctl --help
./scripts/studioctl status
./scripts/studioctl page create
./scripts/studioctl set generatedRows 600
./scripts/studioctl generate \
  --pattern spiral --rows 600 --noise 0.02 --seed 777 \
  --output "$PWD/build/spiral.csv"
./scripts/studioctl profile
./scripts/studioctl train \
  --data "$PWD/build/spiral.csv" \
  --output "$PWD/build/spiral.neuroevo" \
  --population 100 --generations 100
./scripts/studioctl watch --count 20
./scripts/studioctl wait --training-timeout 120
./scripts/studioctl predict "0.2,-0.1"
```

The CLI returns JSON containing `ok`, optional `error`, `roundTripMs`, and the current app `state`. Use `--compact` for JSONL pipelines. Use `--dry-run` before file-producing operations.

## Protocol
The app creates `/tmp/neuroevo-studio-control-<uid>` with mode `0700`. The CLI atomically places a JSON request in that directory. The main-actor app loop accepts only whitelisted commands and settings, updates `StudioModel`, writes a matching reply, and removes the request. There is no socket or network listener.

## Verification
```bash
./scripts/test-live-control --dry-run
./scripts/test-live-control
```

The self-test navigates, changes a visible value, generates and profiles data, trains and waits, predicts, opens Learn, validates output files, and verifies final JSON state.

## Rollback
Revert the live-control commit. Removing `StudioControl.swift`, the `.task` loop, and the CLI scripts does not alter saved datasets or models.

## Troubleshooting
- **App not running:** launch the app as the same user, then retry `studioctl status`.
- **Timeout:** confirm the app is responsive and the control directory belongs to the current user.
- **Setting rejected:** use a supported name; rejection is intentional rather than arbitrary property access.
- **Training finishes before `wait`:** `wait` recognizes an already-finished run and returns its final state.
