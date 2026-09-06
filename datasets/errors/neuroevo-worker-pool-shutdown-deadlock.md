# Error Record: Worker Pool Shutdown Deadlock

## Date
2026-09-06

## Command or Action
Ran a one-generation, 18-candidate stress test on a generated 100,000-row dataset.

## Error Output
The model finished its work but the process did not exit. A process sample showed
the main thread blocked joining a worker while the remaining worker was blocked on
the pool mutex.

## Context
The pool's `std::jthread` vector was declared before its mutex and condition
variables. C++ destroys members in reverse declaration order.

## Confirmed Cause
The destructor only signaled shutdown. Implicit member destruction then destroyed
the synchronization members before destroying and joining the thread vector,
creating undefined behavior and a nondeterministic shutdown deadlock.

## Fix Attempted
After setting the stop flag and notifying all workers, explicitly clear the thread
vector so every worker joins while the mutex and condition variables remain alive.

## Result
The same 100,000-row workload completed and exited normally. Release and sanitizer
tests passed.

## Reusable Lesson
Thread-owning objects must join workers before destroying any state those workers can
still access; do not rely on implicit member destruction unless declaration order
guarantees that lifetime relationship.
