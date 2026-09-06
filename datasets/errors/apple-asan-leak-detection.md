# Error Record: Apple AddressSanitizer Leak Detection

## Date
2026-09-06

## Command or Action
Ran the maximum-topology benchmark from an AddressSanitizer and
UndefinedBehaviorSanitizer build with `ASAN_OPTIONS=detect_leaks=1`.

## Error Output
`AddressSanitizer: detect_leaks is not supported on this platform.`

## Context
The sanitizer unit test had already passed. The extra benchmark run attempted to
enable leak detection explicitly on macOS.

## Confirmed Cause
The Apple platform AddressSanitizer runtime in this environment does not support
LeakSanitizer's `detect_leaks` mode.

## Possible Causes
None recorded; the runtime reported the unsupported option directly.

## Fix Attempted
Reran the benchmark with `ASAN_OPTIONS=detect_leaks=0`, retaining address and
undefined-behavior instrumentation.

## Result
The instrumented maximum-topology benchmark completed successfully.

## Reusable Lesson
On this Apple toolchain, use AddressSanitizer and UndefinedBehaviorSanitizer without
claiming LeakSanitizer coverage. Treat leak verification as not performed unless a
separate supported tool is used.
