# Error Record: Core ML Tools Datatypes Import

## Date
2026-09-06

## Command or Action
Export a version 2 Neuroevo model with Core ML Tools 9.0.

## Error Output
```text
cannot import name 'datatypes' from 'coremltools.models.neural_network'
```

## Context
The first exporter version used an older documented module location for `datatypes`.

## Confirmed Cause
In the installed Core ML Tools 9.0 package, `datatypes` is exported from `coremltools.models`, not `coremltools.models.neural_network`.

## Possible Causes
None retained after package inspection and successful rerun.

## Fix Attempted
Changed the import to `from coremltools.models import datatypes`.

## Result
Export succeeded. The native Core ML verifier loaded the model with CPU and Neural Engine use allowed and produced correct outputs for all four XOR inputs.

## Reusable Lesson
Pin and test model-conversion tooling versions; conversion APIs and module layouts can change independently from the C++ model format.

