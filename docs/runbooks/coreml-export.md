# Runbook: Core ML Export and Verification

## Purpose
Convert a saved Neuroevo model into a Core ML model and validate native inference with CPU and Neural Engine use allowed.

## Prerequisites
- macOS
- Python 3 supported by the selected Core ML Tools version
- A version 2 `.neuroevo` model

## Steps
```bash
python3 -m venv .venv-coreml
source .venv-coreml/bin/activate
python -m pip install coremltools numpy

python tools/export_coreml.py \
  --model build/coreml-xor.neuroevo \
  --output build/coreml-xor.mlmodel

swiftc -parse-as-library tools/verify_coreml.swift \
  -framework CoreML -o build/verify_coreml

./build/verify_coreml build/coreml-xor.mlmodel 0,1
```

## Verification
- Export prints the output path and `CPU_AND_NE allowed`.
- The Swift verifier loads the compiled model with `.cpuAndNeuralEngine`.
- Compare its numeric outputs with `neuroevo --model ... --predict-row ...`.
- “Allowed” is not proof of actual Neural Engine placement; Core ML chooses the device for each compatible graph segment.

## Rollback
Delete the generated `.mlmodel` and compiled verification binary. The source `.neuroevo` model is unchanged.

## Troubleshooting
- `No module named coremltools`: activate `.venv-coreml` or install dependencies inside it.
- Unsupported Python wheel: create an environment using a Python version supported by the current Core ML Tools release.
- Missing output: confirm the model is version 2 and rerun exporter `--dry-run`.
- Compile failure: ensure macOS Command Line Tools are installed and selected.

