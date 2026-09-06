# Error Record: Invalid C Enum Triggered Undefined Behavior

## Date
2026-09-06

## Command or Action
Ran the C bridge edge-case suite under UndefinedBehaviorSanitizer after adding rejection for an invalid task value.

## Error Output
`runtime error: load of value 99, which is not a valid value for type 'NETask'`

## Context
`NEConfig.task` was declared as a C enum. The test represented malformed external input by writing value 99, then called the guarded C bridge. C++ triggered undefined behavior while loading the enum field before the validation switch could reject it.

## Confirmed Cause
A typed enum cannot safely serve as an untrusted ABI storage field when callers may supply arbitrary integer bytes. Validation after loading the invalid enum is too late.

## Fix Attempted
Changed the C-compatible configuration storage field to `int` while retaining the named `NETask` constants. The bridge now switches on the integer and converts only recognized values to the internal C++ enum.

## Result
The sanitizer suite passed after the ABI change, and the Swift release target compiled after explicitly converting the named C constants to the integer storage type.

## Reusable Lesson
Store untrusted enum-like values as a fixed integer at a C/serialization boundary. Validate the integer first, then convert to a strongly typed enum inside the trusted implementation.
