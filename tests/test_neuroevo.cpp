#include "neuroevo/neuroevo.hpp"
#include "neuroevo/data_tools.hpp"
#include "CNeuroevo.h"

#include <algorithm>
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

void testEvaluationConsistency() {
    std::mt19937_64 rng(2026);
    std::uniform_real_distribution<float> value(-1.0F, 1.0F);
    const auto genome = neuroevo::makeRandomGenome(
        neuroevo::TaskType::Classification, 4, 3, {8, 8}, rng);
    neuroevo::Partition partition;
    for (std::size_t row = 0; row < 64; ++row) {
        neuroevo::Sample sample;
        sample.features.resize(4);
        for (float& feature : sample.features) feature = value(rng);
        sample.target.assign(3, 0.0F);
        sample.target[row % 3] = 1.0F;
        partition.samples.push_back(std::move(sample));
    }

    const auto actual = neuroevo::evaluate(genome, partition);
    const neuroevo::Network network(genome);
    std::size_t correct = 0;
    double totalLoss = 0.0;
    for (const auto& sample : partition.samples) {
        const auto prediction = network.predict(sample.features);
        const auto predicted = static_cast<std::size_t>(
            std::max_element(prediction.begin(), prediction.end()) - prediction.begin());
        const auto expected = static_cast<std::size_t>(
            std::max_element(sample.target.begin(), sample.target.end()) - sample.target.begin());
        if (predicted == expected) ++correct;
        totalLoss -= std::log(std::max(1e-7F, prediction[expected]));
    }
    const double expectedScore = static_cast<double>(correct) /
                                 static_cast<double>(partition.samples.size());
    const double expectedLoss = totalLoss / static_cast<double>(partition.samples.size());
    require(std::abs(actual.score - expectedScore) < 1e-12,
            "batched evaluation changed classification score");
    require(std::abs(actual.loss - expectedLoss) < 1e-5,
            "batched evaluation changed classification loss");

    const auto regressionGenome = neuroevo::makeRandomGenome(
        neuroevo::TaskType::Regression, 4, 2, {8, 8}, rng);
    neuroevo::Partition regressionPartition;
    for (std::size_t row = 0; row < 64; ++row) {
        neuroevo::Sample sample;
        sample.features.resize(4);
        sample.target.resize(2);
        for (float& feature : sample.features) feature = value(rng);
        for (float& target : sample.target) target = value(rng);
        regressionPartition.samples.push_back(std::move(sample));
    }
    const auto regressionActual = neuroevo::evaluate(regressionGenome, regressionPartition);
    const neuroevo::Network regressionNetwork(regressionGenome);
    double squaredError = 0.0;
    for (const auto& sample : regressionPartition.samples) {
        const auto prediction = regressionNetwork.predict(sample.features);
        for (std::size_t i = 0; i < prediction.size(); ++i) {
            const double difference = static_cast<double>(prediction[i] - sample.target[i]);
            squaredError += difference * difference;
        }
    }
    const double regressionExpectedLoss = squaredError /
        static_cast<double>(regressionPartition.samples.size() * 2);
    require(std::abs(regressionActual.loss - regressionExpectedLoss) < 1e-5,
            "batched evaluation changed regression loss");
    require(std::abs(regressionActual.score + regressionActual.loss) < 1e-12,
            "regression score is not negative loss");
}

void testWorkerPoolErrorPropagation() {
    neuroevo::Dataset dataset;
    dataset.task = neuroevo::TaskType::Regression;
    dataset.inputSize = 2;
    dataset.outputSize = 1;
    dataset.train.samples.assign(16, neuroevo::Sample{{1.0F}, {0.0F}});
    dataset.validation.samples.assign(16, neuroevo::Sample{{1.0F, 2.0F}, {0.0F}});
    neuroevo::EvolutionOptions options;
    options.populationSize = 8;
    options.eliteCount = 2;
    options.generations = 2;
    options.threads = 4;
    std::ostringstream progress;
    bool threw = false;
    try {
        static_cast<void>(neuroevo::EvolutionEngine(options).run(dataset, progress));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "worker pool did not propagate an evaluation error");
}

void testTopologySchedulingDeterminism() {
    neuroevo::Dataset dataset;
    dataset.task = neuroevo::TaskType::Regression;
    dataset.inputSize = 2;
    dataset.outputSize = 1;
    for (std::size_t row = 0; row < 128; ++row) {
        const float x = static_cast<float>(row) / 127.0F;
        neuroevo::Sample sample{{x, 1.0F - x}, {x * x - 0.25F}};
        if (row < 64) dataset.train.samples.push_back(sample);
        else if (row < 96) dataset.validation.samples.push_back(sample);
        else dataset.test.samples.push_back(sample);
    }

    neuroevo::EvolutionOptions options;
    options.populationSize = 24;
    options.eliteCount = 4;
    options.generations = 8;
    options.patience = 8;
    options.targetScore = 1.0;
    options.topologyMutationRate = 1.0;
    options.initialHidden = 32;
    options.seed = 90210;
    options.threads = 1;
    std::ostringstream sequentialProgress;
    const auto sequential = neuroevo::EvolutionEngine(options).run(dataset, sequentialProgress);

    options.threads = 4;
    std::ostringstream parallelProgress;
    const auto parallel = neuroevo::EvolutionEngine(options).run(dataset, parallelProgress);
    require(sequential.evaluations == parallel.evaluations,
            "topology scheduling changed the evaluation count");
    require(sequential.history.size() == parallel.history.size(),
            "topology scheduling changed the generation count");
    require(sequential.winner.id == parallel.winner.id,
            "topology scheduling changed the selected winner");
    require(sequential.winner.parameterCount() == parallel.winner.parameterCount(),
            "topology scheduling changed the selected topology");
    require(std::abs(sequential.winner.validationScore - parallel.winner.validationScore) < 1e-12,
            "topology scheduling changed the validation score");
}

struct BridgeContext {
    NESession* session = nullptr;
    std::size_t callbacks = 0;
};

void bridgeProgress(const NEProgress* progress, void* opaque) {
    auto* context = static_cast<BridgeContext*>(opaque);
    ++context->callbacks;
    require(progress != nullptr && progress->topology != nullptr, "invalid bridge progress");
    if (progress->generation >= 2) ne_session_cancel(context->session);
}

void testCBridge() {
    const auto dataPath = std::filesystem::temp_directory_path() / "neuroevo_bridge_data.csv";
    const auto modelPath = std::filesystem::temp_directory_path() / "neuroevo_bridge_model.txt";
    {
        std::ofstream file(dataPath);
        for (int i = 0; i < 12; ++i) file << "0,0,0\n0,1,1\n1,0,1\n1,1,0\n";
    }

    NEConfig config{};
    ne_default_config(&config);
    const std::string data = dataPath.string();
    const std::string model = modelPath.string();
    config.data_path = data.c_str();
    config.output_path = model.c_str();
    config.population_size = 32;
    config.elite_count = 4;
    config.generations = 20;
    config.patience = 20;
    config.target_score = 2.0;
    config.threads = 2;

    char error[512]{};
    NEDatasetInfo info{};
    NEConfig invalid{};
    ne_default_config(&invalid);
    require(ne_inspect_dataset(&invalid, &info, error, sizeof(error)) != 0 && error[0] != '\0',
            "bridge did not report invalid configuration");
    error[0] = '\0';
    require(ne_inspect_dataset(&config, &info, error, sizeof(error)) == 0,
            "bridge dataset inspection failed");
    require(info.input_count == 2 && info.output_count == 2, "bridge dataset shape is wrong");

    BridgeContext context;
    context.session = ne_session_create();
    require(context.session != nullptr, "bridge session creation failed");
    NETrainResult training{};
    const int status = ne_train(context.session, &config, bridgeProgress, &context,
                                &training, error, sizeof(error));
    require(status == 0, "bridge training failed");
    require(training.cancelled == 1 && context.callbacks >= 3, "bridge cancellation failed");
    ne_session_destroy(context.session);

    const float features[] = {0.0F, 1.0F};
    NEPrediction prediction{};
    require(ne_predict(model.c_str(), features, 2, &prediction, error, sizeof(error)) == 0,
            "bridge prediction failed");
    require(prediction.count == 2 && prediction.is_classification == 1,
            "bridge prediction metadata is wrong");
    std::filesystem::remove(dataPath);
    std::filesystem::remove(modelPath);
}

void testDataGenerationAndCleaning() {
    const std::vector<neuroevo::DataPattern> patterns{
        neuroevo::DataPattern::Linear, neuroevo::DataPattern::Polynomial,
        neuroevo::DataPattern::Sine, neuroevo::DataPattern::Xor,
        neuroevo::DataPattern::Circles, neuroevo::DataPattern::Clusters,
        neuroevo::DataPattern::Spiral};
    for (std::size_t i = 0; i < patterns.size(); ++i) {
        const auto path = std::filesystem::temp_directory_path() /
                          ("neuroevo_generated_" + std::to_string(i) + ".csv");
        neuroevo::GenerateOptions options;
        options.pattern = patterns[i];
        options.outputPath = path;
        options.rows = 80;
        options.inputCount = 2;
        options.seed = 100 + i;
        const auto report = neuroevo::generateDataset(options);
        require(report.rowsWritten == 80 && !report.formula.empty(), "generation report is invalid");
        const auto task = report.classification
            ? neuroevo::TaskType::Classification : neuroevo::TaskType::Regression;
        const auto dataset = neuroevo::Dataset::loadCsv(path, task, 1, true, 9);
        require(dataset.inputSize == 2 && dataset.train.samples.size() == 56,
                "generated dataset is not trainable");
        std::filesystem::remove(path);
    }

    const auto first = std::filesystem::temp_directory_path() / "neuroevo_seed_first.csv";
    const auto second = std::filesystem::temp_directory_path() / "neuroevo_seed_second.csv";
    neuroevo::GenerateOptions repeatable;
    repeatable.pattern = neuroevo::DataPattern::Circles;
    repeatable.outputPath = first;
    repeatable.rows = 50;
    repeatable.noise = 0.1;
    repeatable.seed = 2026;
    neuroevo::generateDataset(repeatable);
    repeatable.outputPath = second;
    neuroevo::generateDataset(repeatable);
    std::ifstream firstInput(first);
    std::ifstream secondInput(second);
    require(std::string(std::istreambuf_iterator<char>(firstInput), {}) ==
            std::string(std::istreambuf_iterator<char>(secondInput), {}),
            "same seed did not reproduce generated data");
    std::filesystem::remove(first);
    std::filesystem::remove(second);

    const auto bridgeGenerated = std::filesystem::temp_directory_path() / "neuroevo_bridge_generated.csv";
    const auto bridgeCleaned = std::filesystem::temp_directory_path() / "neuroevo_bridge_cleaned.csv";
    const std::string bridgeGeneratedPath = bridgeGenerated.string();
    const std::string bridgeCleanedPath = bridgeCleaned.string();
    NEGenerateConfig bridgeGeneration{};
    bridgeGeneration.output_path = bridgeGeneratedPath.c_str();
    bridgeGeneration.pattern = NE_PATTERN_XOR;
    bridgeGeneration.rows = 64;
    bridgeGeneration.input_count = 2;
    bridgeGeneration.minimum = -1.0;
    bridgeGeneration.maximum = 1.0;
    bridgeGeneration.noise = 0.05;
    bridgeGeneration.seed = 88;
    bridgeGeneration.include_header = 1;
    NEGenerateResult bridgeGenerationResult{};
    char bridgeError[512]{};
    require(ne_generate_dataset(&bridgeGeneration, &bridgeGenerationResult,
                                bridgeError, sizeof(bridgeError)) == 0,
            "C bridge generation failed");
    require(bridgeGenerationResult.rows_written == 64 &&
            bridgeGenerationResult.is_classification == 1,
            "C bridge generation result is wrong");
    NEProfileConfig bridgeProfileConfig{bridgeGeneratedPath.c_str(), 1};
    NEProfileResult bridgeProfile{};
    require(ne_profile_dataset(&bridgeProfileConfig, &bridgeProfile,
                               bridgeError, sizeof(bridgeError)) == 0 && bridgeProfile.rows == 64,
            "C bridge profiling failed");
    NECleanConfig bridgeCleaning{};
    bridgeCleaning.input_path = bridgeGeneratedPath.c_str();
    bridgeCleaning.output_path = bridgeCleanedPath.c_str();
    bridgeCleaning.has_header = 1;
    bridgeCleaning.target_columns = 1;
    bridgeCleaning.impute_missing_features = 1;
    bridgeCleaning.remove_duplicates = 1;
    bridgeCleaning.drop_malformed_rows = 1;
    NECleanResult bridgeCleanResult{};
    require(ne_clean_dataset(&bridgeCleaning, &bridgeCleanResult,
                             bridgeError, sizeof(bridgeError)) == 0 &&
            bridgeCleanResult.rows_written > 0,
            "C bridge cleaning failed");
    std::filesystem::remove(bridgeGenerated);
    std::filesystem::remove(bridgeCleaned);

    const auto dirty = std::filesystem::temp_directory_path() / "neuroevo_dirty.csv";
    const auto clean = std::filesystem::temp_directory_path() / "neuroevo_clean.csv";
    {
        std::ofstream file(dirty);
        file << "a,b,label\n1,2,0\n1,,1\nbad,3,0\n1,2,0\n4,5,?\n";
    }
    const auto profile = neuroevo::profileDataset({dirty, true});
    require(profile.rows == 5 && profile.columns == 3, "profile dimensions are wrong");
    require(profile.missingCells == 2 && profile.malformedRows == 1 &&
            profile.duplicateRows == 1, "profile quality counts are wrong");

    neuroevo::CleanOptions cleaning;
    cleaning.inputPath = dirty;
    cleaning.outputPath = clean;
    const auto cleaned = neuroevo::cleanDataset(cleaning);
    require(cleaned.rowsWritten == 2 && cleaned.rowsDropped == 2,
            "cleaning row counts are wrong");
    require(cleaned.missingValuesImputed == 1 && cleaned.duplicatesRemoved == 1,
            "cleaning operation counts are wrong");
    const auto cleanProfile = neuroevo::profileDataset({clean, true});
    require(cleanProfile.rows == 2 && cleanProfile.missingCells == 0 &&
            cleanProfile.malformedRows == 0, "cleaned output is not numeric and complete");
    std::filesystem::remove(dirty);
    std::filesystem::remove(clean);
}

} // namespace

int main() {
    try {
        testKnownNetwork();
        testMutationValidity();
        testDatasetAndModelRoundTrip();
        testSmallEvolution();
        testEvaluationConsistency();
        testWorkerPoolErrorPropagation();
        testTopologySchedulingDeterminism();
        testCBridge();
        testDataGenerationAndCleaning();
        std::cout << "all tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
