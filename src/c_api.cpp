#include "CNeuroevo.h"
#include "neuroevo/data_tools.hpp"
#include "neuroevo/neuroevo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

struct NESession {
    std::atomic_bool cancelled{false};
};

namespace {

void setError(char* destination, std::size_t capacity, const std::string& message) {
    if (destination == nullptr || capacity == 0) return;
    const std::size_t count = std::min(capacity - 1, message.size());
    std::memcpy(destination, message.data(), count);
    destination[count] = '\0';
}

template <typename Function>
int guarded(char* error, std::size_t errorCapacity, Function&& function) noexcept {
    try {
        if (error != nullptr && errorCapacity > 0) error[0] = '\0';
        function();
        return 0;
    } catch (const std::exception& exception) {
        setError(error, errorCapacity, exception.what());
        return 1;
    } catch (...) {
        setError(error, errorCapacity, "unknown C++ error");
        return 1;
    }
}

void validateConfig(const NEConfig* config) {
    if (config == nullptr) throw std::invalid_argument("configuration is null");
    if (config->data_path == nullptr || config->data_path[0] == '\0') {
        throw std::invalid_argument("choose a CSV data file");
    }
}

neuroevo::TaskType taskFrom(const int task) {
    switch (task) {
        case NE_TASK_CLASSIFICATION: return neuroevo::TaskType::Classification;
        case NE_TASK_REGRESSION: return neuroevo::TaskType::Regression;
    }
    throw std::invalid_argument("unknown task type");
}

neuroevo::DataPattern patternFrom(const NEDataPattern pattern) {
    switch (pattern) {
        case NE_PATTERN_LINEAR: return neuroevo::DataPattern::Linear;
        case NE_PATTERN_POLYNOMIAL: return neuroevo::DataPattern::Polynomial;
        case NE_PATTERN_SINE: return neuroevo::DataPattern::Sine;
        case NE_PATTERN_XOR: return neuroevo::DataPattern::Xor;
        case NE_PATTERN_CIRCLES: return neuroevo::DataPattern::Circles;
        case NE_PATTERN_CLUSTERS: return neuroevo::DataPattern::Clusters;
        case NE_PATTERN_SPIRAL: return neuroevo::DataPattern::Spiral;
    }
    throw std::invalid_argument("unknown data pattern");
}

neuroevo::EvolutionOptions optionsFrom(const NEConfig& config) {
    neuroevo::EvolutionOptions options;
    options.populationSize = config.population_size;
    options.generations = config.generations;
    options.eliteCount = config.elite_count;
    options.tournamentSize = config.tournament_size;
    options.threads = config.threads;
    options.seed = config.seed;
    options.weightMutationRate = config.weight_mutation_rate;
    options.weightMutationSigma = config.weight_mutation_sigma;
    options.topologyMutationRate = config.topology_mutation_rate;
    options.activationMutationRate = config.activation_mutation_rate;
    options.immigrantRate = config.immigrant_rate;
    options.complexityPenalty = config.complexity_penalty;
    options.targetScore = config.target_score;
    options.patience = config.patience;
    options.initialHidden = config.initial_hidden;
    options.maxHiddenLayers = config.max_hidden_layers;
    options.maxLayerWidth = config.max_layer_width;
    return options;
}

neuroevo::Dataset loadDataset(const NEConfig& config) {
    return neuroevo::Dataset::loadCsv(
        std::filesystem::path(config.data_path), taskFrom(config.task),
        config.target_columns, config.has_header != 0, config.seed);
}

std::string topology(const neuroevo::Genome& genome) {
    std::ostringstream output;
    output << genome.layers.front().inputs;
    for (const auto& layer : genome.layers) output << " → " << layer.outputs;
    return output.str();
}

void copyString(char* destination, std::size_t capacity, const std::string& value) {
    if (capacity == 0) return;
    const std::size_t count = std::min(capacity - 1, value.size());
    std::memcpy(destination, value.data(), count);
    destination[count] = '\0';
}

} // namespace

extern "C" {

void ne_default_config(NEConfig* config) {
    if (config == nullptr) return;
    *config = {};
    config->task = NE_TASK_CLASSIFICATION;
    config->target_columns = 1;
    config->population_size = 160;
    config->generations = 150;
    config->elite_count = 8;
    config->tournament_size = 5;
    config->threads = 0;
    config->seed = 42;
    config->weight_mutation_rate = 0.12;
    config->weight_mutation_sigma = 0.35;
    config->topology_mutation_rate = 0.12;
    config->activation_mutation_rate = 0.03;
    config->immigrant_rate = 0.04;
    config->complexity_penalty = 1e-6;
    config->target_score = 0.995;
    config->patience = 40;
    config->initial_hidden = 6;
    config->max_hidden_layers = 4;
    config->max_layer_width = 128;
}

const char* ne_backend_name(void) {
    static const std::string name = neuroevo::backendName();
    return name.c_str();
}

NESession* ne_session_create(void) {
    try {
        return new NESession;
    } catch (...) {
        return nullptr;
    }
}

void ne_session_cancel(NESession* session) {
    if (session != nullptr) session->cancelled.store(true, std::memory_order_relaxed);
}

void ne_session_destroy(NESession* session) {
    delete session;
}

int ne_inspect_dataset(const NEConfig* config,
                       NEDatasetInfo* result,
                       char* error,
                       size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        validateConfig(config);
        if (result == nullptr) throw std::invalid_argument("dataset result is null");
        const auto dataset = loadDataset(*config);
        result->input_count = dataset.inputSize;
        result->output_count = dataset.outputSize;
        result->class_count = dataset.classValues.size();
        result->train_rows = dataset.train.samples.size();
        result->validation_rows = dataset.validation.samples.size();
        result->test_rows = dataset.test.samples.size();
    });
}

int ne_train(NESession* session,
             const NEConfig* config,
             NEProgressCallback callback,
             void* callback_context,
             NETrainResult* result,
             char* error,
             size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        validateConfig(config);
        if (session == nullptr) throw std::invalid_argument("training session is null");
        if (result == nullptr) throw std::invalid_argument("training result is null");
        if (config->output_path == nullptr || config->output_path[0] == '\0') {
            throw std::invalid_argument("choose an output model path");
        }
        session->cancelled.store(false, std::memory_order_relaxed);
        *result = {};
        const auto dataset = loadDataset(*config);
        neuroevo::EvolutionEngine engine(optionsFrom(*config));
        std::ostringstream logSink;
        const auto started = std::chrono::steady_clock::now();
        const auto evolution = engine.run(
            dataset, logSink,
            [&](const neuroevo::GenerationStats& stats, const neuroevo::Genome& best) {
                const std::string shape = topology(best);
                NEProgress progress{};
                progress.generation = stats.generation;
                progress.generation_limit = config->generations;
                progress.best_train_score = stats.bestTrainScore;
                progress.best_validation_score = stats.bestValidationScore;
                progress.mean_fitness = stats.meanFitness;
                progress.parameter_count = stats.bestParameters;
                progress.evaluations = (stats.generation + 1) * config->population_size;
                progress.elapsed_seconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - started).count();
                progress.topology = shape.c_str();
                if (callback != nullptr) callback(&progress, callback_context);
                return !session->cancelled.load(std::memory_order_relaxed);
            });

        const auto evaluation = neuroevo::evaluateAgainstBaseline(
            evolution.winner, dataset.train, dataset.test);
        neuroevo::SavedModel{evolution.winner, dataset.featureMean, dataset.featureScale,
                             dataset.targetMean, dataset.targetScale,
                             dataset.classValues}.save(config->output_path);
        result->cancelled = session->cancelled.load(std::memory_order_relaxed) ? 1 : 0;
        result->validation_score = evolution.winner.validationScore;
        result->test_score = evaluation.model.score;
        result->test_loss = evaluation.model.loss;
        result->baseline_score = evaluation.baseline.score;
        result->baseline_loss = evaluation.baseline.loss;
        result->improvement_over_baseline = evaluation.improvementOverBaseline;
        result->balanced_accuracy = evaluation.balancedAccuracy;
        result->mean_absolute_error = evaluation.meanAbsoluteError;
        result->r_squared = evaluation.rSquared;
        result->parameter_count = evolution.winner.parameterCount();
        result->evaluations = evolution.evaluations;
        result->elapsed_seconds = evolution.elapsedSeconds;
        copyString(result->topology, sizeof(result->topology), topology(evolution.winner));
    });
}

int ne_predict(const char* model_path,
               const float* features,
               size_t feature_count,
               NEPrediction* result,
               char* error,
               size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        if (model_path == nullptr || model_path[0] == '\0') {
            throw std::invalid_argument("model path is empty");
        }
        if (features == nullptr || feature_count == 0) {
            throw std::invalid_argument("prediction features are empty");
        }
        if (result == nullptr) throw std::invalid_argument("prediction result is null");
        const auto model = neuroevo::SavedModel::load(model_path);
        const auto prediction = model.predictRaw(std::vector<float>(features, features + feature_count));
        if (prediction.size() > 256) throw std::runtime_error("model has more than 256 outputs");
        *result = {};
        result->count = prediction.size();
        result->is_classification = model.genome.task == neuroevo::TaskType::Classification ? 1 : 0;
        for (std::size_t i = 0; i < prediction.size(); ++i) result->values[i] = prediction[i];
        if (result->is_classification != 0) {
            const std::size_t index = static_cast<std::size_t>(
                std::max_element(prediction.begin(), prediction.end()) - prediction.begin());
            result->class_value = model.classValues.at(index);
        }
    });
}

double ne_prediction_value(const NEPrediction* prediction, size_t index) {
    if (prediction == nullptr || index >= prediction->count || index >= 256) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return prediction->values[index];
}

int ne_generate_dataset(const NEGenerateConfig* config,
                        NEGenerateResult* result,
                        char* error,
                        size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        if (config == nullptr || result == nullptr) {
            throw std::invalid_argument("generation configuration or result is null");
        }
        neuroevo::GenerateOptions options;
        options.outputPath = config->output_path == nullptr ? "" : config->output_path;
        options.pattern = patternFrom(config->pattern);
        options.rows = config->rows;
        options.inputCount = config->input_count;
        options.minimum = config->minimum;
        options.maximum = config->maximum;
        options.noise = config->noise;
        options.seed = config->seed;
        options.includeHeader = config->include_header != 0;
        const auto generated = neuroevo::generateDataset(options);
        *result = {};
        result->rows_written = generated.rowsWritten;
        result->input_count = generated.inputCount;
        result->output_count = generated.outputCount;
        result->is_classification = generated.classification ? 1 : 0;
        copyString(result->formula, sizeof(result->formula), generated.formula);
    });
}

int ne_profile_dataset(const NEProfileConfig* config,
                       NEProfileResult* result,
                       char* error,
                       size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        if (config == nullptr || result == nullptr || config->input_path == nullptr) {
            throw std::invalid_argument("profile configuration is incomplete");
        }
        const auto profile = neuroevo::profileDataset(
            {std::filesystem::path(config->input_path), config->has_header != 0});
        *result = {profile.rows, profile.columns, profile.completeRows, profile.missingCells,
                   profile.malformedRows, profile.duplicateRows};
    });
}

int ne_clean_dataset(const NECleanConfig* config,
                     NECleanResult* result,
                     char* error,
                     size_t error_capacity) {
    return guarded(error, error_capacity, [&] {
        if (config == nullptr || result == nullptr || config->input_path == nullptr ||
            config->output_path == nullptr) {
            throw std::invalid_argument("cleaning configuration is incomplete");
        }
        neuroevo::CleanOptions options;
        options.inputPath = config->input_path;
        options.outputPath = config->output_path;
        options.hasHeader = config->has_header != 0;
        options.targetColumns = config->target_columns;
        options.imputeMissingFeatures = config->impute_missing_features != 0;
        options.removeDuplicates = config->remove_duplicates != 0;
        options.dropMalformedRows = config->drop_malformed_rows != 0;
        options.clipZScore = config->clip_z_score;
        const auto cleaned = neuroevo::cleanDataset(options);
        *result = {cleaned.rowsRead, cleaned.rowsWritten, cleaned.rowsDropped,
                   cleaned.missingValuesImputed, cleaned.duplicatesRemoved,
                   cleaned.valuesClipped};
    });
}

} // extern "C"
