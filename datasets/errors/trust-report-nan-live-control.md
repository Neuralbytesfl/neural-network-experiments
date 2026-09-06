# Error Record: Trust Report NaN Live Control Failure

## Date
2026-09-06

## Command or Action
Ran the packaged native app and exercised Create → Profile → Train → Predict → Learn using `scripts/test-live-control`.

## Error Output
`NSInvalidArgumentException: Invalid number value (NaN) in JSON write`

## Context
Task-specific C++ metrics use NaN to represent “not applicable.” A classification result therefore had valid balanced accuracy and non-applicable regression MAE/R-squared. The GUI rendered only the applicable values, but live-control state attempted to serialize every `Double` directly.

## Confirmed Cause
Foundation JSON serialization does not accept NaN. The control response builder passed non-finite task-specific metrics into `JSONSerialization`.

## Fix Attempted
Added a single `jsonMetric` boundary helper that converts absent or non-finite metrics to JSON `null`, while retaining finite values. Extended the live-control integration test to require finite classification trust metrics.

## Result
The rebuilt packaged app completed the full live-control workflow. Its state response contained finite classification metrics and JSON `null` for regression-only metrics.

## Reusable Lesson
Internal numeric sentinels must be normalized at serialization boundaries. JSON, property lists, databases, and charting layers should receive an explicit optional/null representation rather than NaN or infinity.
