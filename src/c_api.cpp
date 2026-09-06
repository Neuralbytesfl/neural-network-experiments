#include "CNeuroevo.h"
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

neuroevo::TaskType taskFrom(const NETask task) {
    return task == NE_TASK_REGRESSION
        ? neuroevo::TaskType::Regression
        : neuroevo::TaskType::Classification;
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

        const auto test = neuroevo::evaluate(evolution.winner, dataset.test);
        neuroevo::SavedModel{evolution.winner, dataset.featureMean, dataset.featureScale,
                             dataset.targetMean, dataset.targetScale,
                             dataset.classValues}.save(config->output_path);
        result->cancelled = session->cancelled.load(std::memory_order_relaxed) ? 1 : 0;
        result->validation_score = evolution.winner.validationScore;
        result->test_score = test.score;
        result->test_loss = test.loss;
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

} // extern "C"
