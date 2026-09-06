# Benchmark: Live CLI-to-GUI Control

## Date
2026-09-06

## System
Apple M5 Pro MacBook Pro, 18 CPU cores, 24 GB memory, macOS 26.6.2. Packaged release app using a 100 ms command polling interval.

## Baseline
GUI automation required accessibility discovery and visual interaction. There was no direct way to query or mutate a running app.

## Change
Added an atomic per-user JSON command/reply queue and the `studioctl` client.

## After
Twenty separate `studioctl --compact status` processes against the packaged app:

- Mean: 64.930 ms
- Median: 71.834 ms
- 95th percentile: 75.875 ms
- Minimum: 49.059 ms
- Maximum: 99.270 ms

## Result
Commands reflected in the GUI within the 100 ms design target in the measured run. CLI process startup is included.

## Notes
This is one local run. Scheduler load and poll phase affect latency. Long operations such as generation, profiling, and training add their own work time; training progress remains asynchronous and can be followed with `watch`.
