#!/usr/bin/env python3
"""Export a Neuroevo v2 text model to a Core ML neural-network model."""

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass


@dataclass
class Layer:
    inputs: int
    outputs: int
    activation: str
    weights: list[float]
    biases: list[float]


@dataclass
class Model:
    task: str
    means: list[float]
    scales: list[float]
    target_means: list[float]
    target_scales: list[float]
    classes: list[float]
    layers: list[Layer]


class Tokens:
    def __init__(self, path: pathlib.Path) -> None:
        self.values = path.read_text(encoding="utf-8").split()
        self.index = 0

    def take(self) -> str:
        if self.index >= len(self.values):
            raise ValueError("unexpected end of model")
        value = self.values[self.index]
        self.index += 1
        return value

    def expect(self, expected: str) -> None:
        actual = self.take()
        if actual != expected:
            raise ValueError(f"expected {expected!r}, found {actual!r}")

    def vector(self, name: str) -> list[float]:
        self.expect(name)
        return [float(self.take()) for _ in range(int(self.take()))]


def load_model(path: pathlib.Path) -> Model:
    tokens = Tokens(path)
    tokens.expect("NEUROEVO_MODEL")
    if int(tokens.take()) != 2:
        raise ValueError("only Neuroevo model version 2 is supported")
    tokens.expect("task")
    task = tokens.take()
    means = tokens.vector("means")
    scales = tokens.vector("scales")
    target_means = tokens.vector("target_means")
    target_scales = tokens.vector("target_scales")
    classes = tokens.vector("classes")
    tokens.expect("layers")
    layers: list[Layer] = []
    for _ in range(int(tokens.take())):
        tokens.expect("layer")
        inputs, outputs, activation = int(tokens.take()), int(tokens.take()), tokens.take()
        weights = tokens.vector("weights")
        biases = tokens.vector("biases")
        if len(weights) != inputs * outputs or len(biases) != outputs:
            raise ValueError("invalid layer parameter dimensions")
        layers.append(Layer(inputs, outputs, activation, weights, biases))
    tokens.expect("end")
    return Model(task, means, scales, target_means, target_scales, classes, layers)


def bake_normalization(model: Model, numpy_module) -> tuple[list, list]:
    np = numpy_module
    weights = [np.asarray(layer.weights, dtype=np.float32).reshape(layer.outputs, layer.inputs)
               for layer in model.layers]
    biases = [np.asarray(layer.biases, dtype=np.float32) for layer in model.layers]
    means = np.asarray(model.means, dtype=np.float32)
    scales = np.asarray(model.scales, dtype=np.float32)
    weights[0] = weights[0] / scales[np.newaxis, :]
    biases[0] = biases[0] - weights[0] @ means
    if model.task == "regression":
        target_means = np.asarray(model.target_means, dtype=np.float32)
        target_scales = np.asarray(model.target_scales, dtype=np.float32)
        weights[-1] = weights[-1] * target_scales[:, np.newaxis]
        biases[-1] = biases[-1] * target_scales + target_means
    return weights, biases


def export(model: Model, output: pathlib.Path) -> None:
    try:
        import coremltools as ct
        import numpy as np
        from coremltools.models import datatypes
        from coremltools.models.neural_network import NeuralNetworkBuilder
    except ModuleNotFoundError as error:
        raise RuntimeError(
            "Core ML export needs an isolated Python environment with coremltools and numpy. "
            "Create one with: python3 -m venv .venv-coreml && "
            "source .venv-coreml/bin/activate && python -m pip install coremltools numpy"
        ) from error

    if not model.layers:
        raise ValueError("model has no layers")
    weights, biases = bake_normalization(model, np)
    builder = NeuralNetworkBuilder(
        input_features=[("features", datatypes.Array(model.layers[0].inputs))],
        output_features=[("prediction", datatypes.Array(model.layers[-1].outputs))],
    )
    current = "features"
    activation_names = {"tanh": "TANH", "relu": "RELU", "leaky_relu": "LEAKYRELU"}
    for index, layer in enumerate(model.layers):
        dense_output = f"dense_{index}"
        builder.add_inner_product(
            name=f"dense_{index}", W=weights[index], b=biases[index],
            input_channels=layer.inputs, output_channels=layer.outputs,
            has_bias=True, input_name=current, output_name=dense_output,
        )
        is_last = index + 1 == len(model.layers)
        if not is_last:
            activated = f"activation_{index}"
            params = [0.01] if layer.activation == "leaky_relu" else None
            builder.add_activation(
                name=activated, non_linearity=activation_names[layer.activation],
                input_name=dense_output, output_name=activated, params=params,
            )
            current = activated
        elif model.task == "classification":
            builder.add_softmax(name="probabilities", input_name=dense_output,
                                output_name="prediction")
            current = "prediction"
        else:
            builder.add_activation(name="prediction", non_linearity="LINEAR",
                                   input_name=dense_output, output_name="prediction")
            current = "prediction"

    builder.spec.description.metadata.shortDescription = "Evolved by Neuroevo Studio"
    builder.spec.description.metadata.author = "Neuroevo Studio"
    coreml_model = ct.models.MLModel(builder.spec, compute_units=ct.ComputeUnit.CPU_AND_NE)
    output.parent.mkdir(parents=True, exist_ok=True)
    coreml_model.save(str(output))
    print(f"Exported {output} with CPU_AND_NE allowed ({len(model.layers)} dense layers)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, type=pathlib.Path,
                        help="Input .neuroevo version 2 model")
    parser.add_argument("--output", required=True, type=pathlib.Path,
                        help="Output .mlmodel path")
    parser.add_argument("--dry-run", action="store_true",
                        help="Parse and describe the model without importing Core ML Tools")
    args = parser.parse_args()
    model = load_model(args.model)
    if args.dry_run:
        shape = [model.layers[0].inputs] + [layer.outputs for layer in model.layers]
        print(f"task={model.task} topology={'x'.join(map(str, shape))} output={args.output}")
        return 0
    export(model, args.output)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(2)
