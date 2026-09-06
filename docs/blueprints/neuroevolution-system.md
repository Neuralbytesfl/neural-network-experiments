# Blueprint: Neuroevolution System

## Purpose
Search for a useful neural network without requiring the user to choose its hidden-layer layout or initial weights manually.

## Components
- `Dataset`: numeric CSV ingestion, label encoding, splitting, and normalization.
- `Genome`: serializable architecture, activation choices, weights, biases, fitness, and lineage.
- `Network`: validated executable phenotype built from a genome.
- `Evaluator`: task-specific loss and score computation.
- `EvolutionEngine`: population initialization, selection, elitism, crossover, mutation, and stopping.
- `Backend`: scalar portable math or Apple Accelerate.
- `CLI`: configuration, dry-run inspection, training, reporting, and checkpoint output.

## Data Flow
```text
CSV -> parse -> split -> train-only normalization
                            |
                            v
Genome population -> Network builder -> validation score
       ^                                  |
       |                                  v
       +------ selection/mutation/crossover
                            |
                            v
                  winner -> held-out test -> saved genome
```

## Control Flow
1. Validate CLI and CSV.
2. Derive input and output dimensions.
3. Create a minimal randomized population.
4. Evaluate candidates in parallel.
5. Preserve elites and create offspring.
6. Stop on target score or generation budget.
7. Evaluate the winner on the untouched test partition and save it.

## Configuration
Population size, generations, seed, thread count, task type, regression target count, split ratios, mutation rates, complexity penalty, and target score are command-line options.

## Security
The program reads local numeric CSV and writes a model file. It does not execute input content, use network access, or store secrets.

## Failure Modes
- Malformed or nonnumeric CSV.
- Too few rows for three partitions.
- Invalid label/target configuration.
- Overfitting to the validation partition.
- Premature population convergence.
- Search budget too small for the problem.

## Recovery
Use a saved seed/configuration to reproduce a run. Retain the previous winner before replacing a deployed model.

## Verification
Unit tests cover parsing, mathematical behavior, topology mutation validity, deterministic runs, and an end-to-end learnable dataset.

## Benchmark
Report candidate evaluations per second, elapsed search time, parameter count, validation metric, and held-out test metric.

## Reuse Points
Fitness evaluation can be replaced for control, simulation, or multi-objective tasks while retaining the genome and evolution engine.
