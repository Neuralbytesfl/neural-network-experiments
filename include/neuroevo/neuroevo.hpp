#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace neuroevo {

enum class TaskType { Classification, Regression };
enum class Activation { Tanh, Relu, LeakyRelu };

std::string toString(TaskType value);
std::string toString(Activation value);
TaskType parseTask(const std::string& value);
Activation parseActivation(const std::string& value);

struct Sample {
    std::vector<float> features;
    std::vector<float> target;
};

struct Partition {
    std::vector<Sample> samples;
};

struct Dataset {
    TaskType task = TaskType::Classification;
    std::size_t inputSize = 0;
    std::size_t outputSize = 0;
    Partition train;
    Partition validation;
    Partition test;
    std::vector<float> featureMean;
    std::vector<float> featureScale;
    std::vector<float> targetMean;
    std::vector<float> targetScale;
    std::vector<float> classValues;

    static Dataset loadCsv(const std::filesystem::path& path,
                           TaskType task,
                           std::size_t targetColumns,
                           bool hasHeader,
                           std::uint64_t seed,
                           double trainRatio = 0.70,
                           double validationRatio = 0.15);
};

struct LayerGene {
    std::size_t inputs = 0;
    std::size_t outputs = 0;
    Activation activation = Activation::Tanh;
    std::vector<float> weights; // output-major: [output][input]
    std::vector<float> biases;
};

struct Genome {
    std::uint64_t id = 0;
    std::uint64_t parentA = 0;
    std::uint64_t parentB = 0;
    std::size_t generation = 0;
    TaskType task = TaskType::Classification;
    std::vector<LayerGene> layers;
    double fitness = 0.0;
    double trainScore = 0.0;
    double validationScore = 0.0;
    double trainLoss = 0.0;
    double validationLoss = 0.0;

    [[nodiscard]] std::size_t parameterCount() const;
    [[nodiscard]] std::vector<std::size_t> hiddenShape() const;
    void validate() const;
};

class Network {
public:
    explicit Network(const Genome& genome);
    [[nodiscard]] std::vector<float> predict(const std::vector<float>& input) const;

private:
    TaskType task_;
    std::vector<LayerGene> layers_;
};

struct Metric {
    double score = 0.0; // accuracy for classification, negative MSE for regression
    double loss = 0.0;  // cross entropy or MSE
};

Metric evaluate(const Genome& genome, const Partition& partition);

struct EvaluationReport {
    Metric model;
    Metric baseline;
    double improvementOverBaseline = 0.0; // accuracy points or MSE reduction
    double balancedAccuracy = std::numeric_limits<double>::quiet_NaN(); // classification only
    double meanAbsoluteError = std::numeric_limits<double>::quiet_NaN(); // regression only
    double rSquared = std::numeric_limits<double>::quiet_NaN();          // regression only
};

EvaluationReport evaluateAgainstBaseline(const Genome& genome,
                                         const Partition& training,
                                         const Partition& evaluation);

struct EvolutionOptions {
    std::size_t populationSize = 160;
    std::size_t generations = 150;
    std::size_t eliteCount = 8;
    std::size_t tournamentSize = 5;
    std::size_t threads = 0;
    std::uint64_t seed = 42;
    double weightMutationRate = 0.12;
    double weightMutationSigma = 0.35;
    double topologyMutationRate = 0.12;
    double activationMutationRate = 0.03;
    double immigrantRate = 0.04;
    double complexityPenalty = 1e-6;
    double targetScore = 0.995;
    std::size_t patience = 40;
    std::size_t initialHidden = 6;
    std::size_t maxHiddenLayers = 4;
    std::size_t maxLayerWidth = 128;
};

struct GenerationStats {
    std::size_t generation = 0;
    double bestTrainScore = 0.0;
    double bestValidationScore = 0.0;
    double meanFitness = 0.0;
    std::size_t bestParameters = 0;
};

struct EvolutionResult {
    Genome winner;
    std::vector<GenerationStats> history;
    std::size_t evaluations = 0;
    double elapsedSeconds = 0.0;
};

class EvolutionEngine {
public:
    using ProgressCallback = std::function<bool(const GenerationStats&, const Genome&)>;

    explicit EvolutionEngine(EvolutionOptions options);
    EvolutionResult run(const Dataset& dataset,
                        std::ostream& progress,
                        const ProgressCallback& callback = {});

private:
    EvolutionOptions options_;
};

struct SavedModel {
    Genome genome;
    std::vector<float> featureMean;
    std::vector<float> featureScale;
    std::vector<float> targetMean;
    std::vector<float> targetScale;
    std::vector<float> classValues;

    void save(const std::filesystem::path& path) const;
    static SavedModel load(const std::filesystem::path& path);
    [[nodiscard]] std::vector<float> predictRaw(std::vector<float> features) const;
};

Genome makeRandomGenome(TaskType task,
                        std::size_t inputs,
                        std::size_t outputs,
                        const std::vector<std::size_t>& hidden,
                        std::mt19937_64& rng);

void mutateGenome(Genome& genome,
                  const EvolutionOptions& options,
                  std::mt19937_64& rng);

std::string backendName();

} // namespace neuroevo
