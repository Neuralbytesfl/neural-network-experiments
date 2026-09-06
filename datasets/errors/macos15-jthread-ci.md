# Error Record: macOS 15 CI Lacked std::jthread

## Date
2026-09-06

## Command or Action
Pushed the new Quality Gate and inspected its first GitHub-hosted macOS 15 run.

## Error Output
`error: no member named 'jthread' in namespace 'std'`

## Context
Local AppleClang and the Linux runner built the C++20 worker pool. The GitHub macOS 15 runner used an older Apple standard library without `std::jthread`, so its Accelerate build stopped before Swift compilation.

## Confirmed Cause
The engine and training benchmark depended on a C++20 library facility that was not present in the oldest selected Apple CI environment, even though the compiler accepted the configured C++20 language mode. The first fix covered the production worker pool; the second CI run exposed the same dependency in the benchmark target.

## Fix Attempted
Replaced `std::jthread` with `std::thread` in both the production pool and benchmark. The pool retains its explicit stop flag, and both call sites explicitly join every joinable worker before reading results or destroying synchronization state.

## Result
Pending the follow-up GitHub Actions run at the time this record was created.

## Reusable Lesson
Language-standard selection does not guarantee equal standard-library coverage on older Apple runners. Test the minimum supported Apple toolchain and prefer a small compatibility implementation when cancellation tokens are not needed.
