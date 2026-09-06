#include "neuroevo/neuroevo.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testKnownNetwork() {
    neuroevo::Genome genome;
    genome.task = neuroevo::TaskType::Regression;
    genome.layers.push_back({2, 1, neuroevo::Activation::Tanh, {2.0F, -1.0F}, {0.5F}});
    const auto result = neuroevo::Network(genome).predict({3.0F, 4.0F});
    require(result.size() == 1, "unexpected output dimension");
    require(std::abs(result[0] - 2.5F) < 1e-6F, "linear network result is wrong");

    const neuroevo::SavedModel model{genome, {1.0F, 2.0F}, {2.0F, 4.0F},
                                      {10.0F}, {3.0F}, {}};
    const auto raw = model.predictRaw({5.0F, 6.0F});
    require(std::abs(raw[0] - 20.5F) < 1e-6F,
            "regression normalization or denormalization is wrong");
}

void testMutationValidity() {
    std::mt19937_64 rng(123);
    neuroevo::Genome genome = neuroevo::makeRandomGenome(
        neuroevo::TaskType::Classification, 3, 2, {4}, rng);
    neuroevo::EvolutionOptions options;
    options.topologyMutationRate = 1.0;
    options.activationMutationRate = 1.0;
    for (int i = 0; i < 100; ++i) {
        neuroevo::mutateGenome(genome, options, rng);
        genome.validate();
        require(genome.layers.front().inputs == 3, "mutation changed input size");
        require(genome.layers.back().outputs == 2, "mutation changed output size");
    }
}

void testDatasetAndModelRoundTrip() {
    const auto base = std::filesystem::temp_directory_path() / "neuroevo_test_data.csv";
    const auto modelPath = std::filesystem::temp_directory_path() / "neuroevo_test_model.txt";
    {
        std::ofstream file(base);
        for (int i = 0; i < 6; ++i) {
            file << "0,0,0\n0,1,1\n1,0,1\n1,1,0\n";
        }
    }
    const auto dataset = neuroevo::Dataset::loadCsv(
        base, neuroevo::TaskType::Classification, 1, false, 9);
    require(dataset.inputSize == 2 && dataset.outputSize == 2, "dataset shape is wrong");
    require(!dataset.train.samples.empty() && !dataset.validation.samples.empty() &&
            !dataset.test.samples.empty(), "dataset split is empty");

    std::mt19937_64 rng(5);
    auto genome = neuroevo::makeRandomGenome(
        neuroevo::TaskType::Classification, 2, 2, {3}, rng);
    neuroevo::SavedModel original{genome, dataset.featureMean, dataset.featureScale,
                                  dataset.targetMean, dataset.targetScale,
                                  dataset.classValues};
    original.save(modelPath);
    const auto loaded = neuroevo::SavedModel::load(modelPath);
    const auto before = original.predictRaw({0.0F, 1.0F});
    const auto after = loaded.predictRaw({0.0F, 1.0F});
    require(before.size() == after.size(), "model round trip changed output size");
    for (std::size_t i = 0; i < before.size(); ++i) {
        require(std::abs(before[i] - after[i]) < 1e-6F, "model round trip changed prediction");
    }
    std::filesystem::remove(base);
    std::filesystem::remove(modelPath);
}

void testSmallEvolution() {
    const auto path = std::filesystem::temp_directory_path() / "neuroevo_test_or.csv";
    {
        std::ofstream file(path);
        for (int i = 0; i < 20; ++i) {
            file << "0,0,0\n0,1,1\n1,0,1\n1,1,1\n";
        }
    }
    const auto dataset = neuroevo::Dataset::loadCsv(
        path, neuroevo::TaskType::Classification, 1, false, 77);
    neuroevo::EvolutionOptions options;
    options.populationSize = 48;
    options.eliteCount = 4;
    options.generations = 30;
    options.patience = 15;
    options.targetScore = 1.0;
    options.seed = 77;
    options.threads = 2;
    std::ostringstream progress;
    const auto result = neuroevo::EvolutionEngine(options).run(dataset, progress);
    require(result.winner.validationScore >= 0.75, "small evolution failed to learn OR");
    require(result.evaluations > 0, "no candidates evaluated");
    std::filesystem::remove(path);
}

} // namespace

int main() {
    try {
        testKnownNetwork();
        testMutationValidity();
        testDatasetAndModelRoundTrip();
        testSmallEvolution();
        std::cout << "all tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
