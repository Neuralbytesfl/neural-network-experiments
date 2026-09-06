#include "neuroevo/neuroevo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#if defined(NEUROEVO_USE_ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace neuroevo {
namespace {

constexpr float kEpsilon = 1e-7F;

std::vector<float> parseNumericRow(const std::string& line, std::size_t lineNumber) {
    std::vector<float> values;
    std::stringstream stream(line);
    std::string cell;
    while (std::getline(stream, cell, ',')) {
        const auto first = cell.find_first_not_of(" \t\r");
        const auto last = cell.find_last_not_of(" \t\r");
        if (first == std::string::npos) {
            throw std::runtime_error("empty CSV cell at line " + std::to_string(lineNumber));
        }
        const std::string trimmed = cell.substr(first, last - first + 1);
        std::size_t consumed = 0;
        try {
            const float value = std::stof(trimmed, &consumed);
            if (consumed != trimmed.size() || !std::isfinite(value)) {
                throw std::runtime_error("not finite or trailing characters");
            }
            values.push_back(value);
        } catch (const std::exception&) {
            throw std::runtime_error("invalid numeric CSV cell '" + trimmed +
                                     "' at line " + std::to_string(lineNumber));
        }
    }
    return values;
}

float activate(float value, Activation activation) {
    switch (activation) {
        case Activation::Tanh: return std::tanh(value);
        case Activation::Relu: return std::max(0.0F, value);
        case Activation::LeakyRelu: return value >= 0.0F ? value : 0.01F * value;
    }
    return value;
}

void softmax(std::vector<float>& values) {
    const float maximum = *std::max_element(values.begin(), values.end());
    float sum = 0.0F;
    for (float& value : values) {
        value = std::exp(value - maximum);
        sum += value;
    }
    if (!(sum > 0.0F) || !std::isfinite(sum)) {
        const float uniform = 1.0F / static_cast<float>(values.size());
        std::fill(values.begin(), values.end(), uniform);
        return;
    }
    for (float& value : values) value /= sum;
}

std::size_t maximumLayerWidth(const std::vector<LayerGene>& layers) {
    std::size_t maximum = layers.front().inputs;
    for (const auto& layer : layers) maximum = std::max(maximum, layer.outputs);
    return maximum;
}

void predictWithWorkspace(const std::vector<LayerGene>& layers,
                          TaskType task,
                          const std::vector<float>& input,
                          std::vector<float>& current,
                          std::vector<float>& output) {
    if (input.size() != layers.front().inputs) {
        throw std::invalid_argument("input dimension mismatch");
    }
    current.assign(input.begin(), input.end());
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
        const auto& layer = layers[layerIndex];
        output.assign(layer.biases.begin(), layer.biases.end());
#if defined(NEUROEVO_USE_ACCELERATE)
        // BLAS call overhead dominates tiny layers, so retain a scalar fast path.
        if (layer.inputs * layer.outputs >= 256) {
            cblas_sgemv(CblasRowMajor, CblasNoTrans,
                        static_cast<int>(layer.outputs), static_cast<int>(layer.inputs),
                        1.0F, layer.weights.data(), static_cast<int>(layer.inputs),
                        current.data(), 1, 1.0F, output.data(), 1);
        } else
#endif
        {
            for (std::size_t out = 0; out < layer.outputs; ++out) {
                for (std::size_t in = 0; in < layer.inputs; ++in) {
                    output[out] += layer.weights[out * layer.inputs + in] * current[in];
                }
            }
        }
        if (layerIndex + 1 < layers.size()) {
            for (float& value : output) value = activate(value, layer.activation);
        }
        current.swap(output);
    }
    if (task == TaskType::Classification) softmax(current);
}

#if defined(NEUROEVO_USE_ACCELERATE)
struct PackedPartition {
    std::size_t rows = 0;
    std::size_t inputs = 0;
    std::size_t outputs = 0;
    std::vector<float> features;
    std::vector<float> targets;
};

struct AcceleratedWorkspace {
    std::vector<float> current;
    std::vector<float> output;
};

AcceleratedWorkspace& acceleratedWorkspace() {
    thread_local AcceleratedWorkspace workspace;
    return workspace;
}

PackedPartition packPartition(const Partition& partition,
                              std::size_t inputs,
                              std::size_t outputs) {
    PackedPartition packed;
    packed.rows = partition.samples.size();
    packed.inputs = inputs;
    packed.outputs = outputs;
    packed.features.resize(packed.rows * inputs);
    packed.targets.resize(packed.rows * outputs);
    for (std::size_t row = 0; row < packed.rows; ++row) {
        const auto& sample = partition.samples[row];
        if (sample.features.size() != inputs) throw std::invalid_argument("input dimension mismatch");
        if (sample.target.size() != outputs) throw std::invalid_argument("target dimension mismatch");
        std::copy(sample.features.begin(), sample.features.end(),
                  packed.features.begin() + static_cast<std::ptrdiff_t>(row * inputs));
        std::copy(sample.target.begin(), sample.target.end(),
                  packed.targets.begin() + static_cast<std::ptrdiff_t>(row * outputs));
    }
    return packed;
}

Metric evaluateBatchAccelerated(const Genome& genome,
                                const PackedPartition& partition,
                                AcceleratedWorkspace& workspace) {
    const std::size_t rows = partition.rows;
    constexpr std::size_t kMaximumBatchElements = 1U << 20U;
    const std::size_t maximumWidth = maximumLayerWidth(genome.layers);
    const std::size_t batchRows = std::min(
        rows, std::max<std::size_t>(1, kMaximumBatchElements / maximumWidth));
    auto& current = workspace.current;
    auto& output = workspace.output;
    Metric metric;
    std::size_t correct = 0;
    double totalLoss = 0.0;
    const std::size_t outputs = genome.layers.back().outputs;
    for (std::size_t offset = 0; offset < rows; offset += batchRows) {
        const std::size_t count = std::min(batchRows, rows - offset);
        const std::size_t inputs = genome.layers.front().inputs;
        const float* layerInput = partition.features.data() + offset * inputs;

        for (std::size_t layerIndex = 0; layerIndex < genome.layers.size(); ++layerIndex) {
            const auto& layer = genome.layers[layerIndex];
            output.resize(count * layer.outputs);
            for (std::size_t row = 0; row < count; ++row) {
                std::copy(layer.biases.begin(), layer.biases.end(),
                          output.begin() + static_cast<std::ptrdiff_t>(row * layer.outputs));
            }
            cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                        static_cast<int>(count), static_cast<int>(layer.outputs),
                        static_cast<int>(layer.inputs), 1.0F,
                        layerInput, static_cast<int>(layer.inputs),
                        layer.weights.data(), static_cast<int>(layer.inputs),
                        1.0F, output.data(), static_cast<int>(layer.outputs));
            if (layerIndex + 1 < genome.layers.size()) {
                for (float& value : output) value = activate(value, layer.activation);
            }
            current.swap(output);
            layerInput = current.data();
        }

        for (std::size_t row = 0; row < count; ++row) {
            float* prediction = current.data() + row * outputs;
            const float* target = partition.targets.data() + (offset + row) * outputs;
            if (genome.task == TaskType::Classification) {
                const std::size_t predicted = static_cast<std::size_t>(
                    std::max_element(prediction, prediction + outputs) - prediction);
                const std::size_t expected = static_cast<std::size_t>(
                    std::max_element(target, target + outputs) - target);
                if (predicted == expected) ++correct;
                const float maximum = *std::max_element(prediction, prediction + outputs);
                float sum = 0.0F;
                for (std::size_t i = 0; i < outputs; ++i) {
                    prediction[i] = std::exp(prediction[i] - maximum);
                    sum += prediction[i];
                }
                const float expectedProbability = (!(sum > 0.0F) || !std::isfinite(sum))
                    ? 1.0F / static_cast<float>(outputs) : prediction[expected] / sum;
                totalLoss -= std::log(std::max(kEpsilon, expectedProbability));
            } else {
                for (std::size_t i = 0; i < outputs; ++i) {
                    const double difference = static_cast<double>(prediction[i] - target[i]);
                    totalLoss += difference * difference;
                }
            }
        }
    }
    const double count = static_cast<double>(rows);
    if (genome.task == TaskType::Classification) {
        metric.score = static_cast<double>(correct) / count;
        metric.loss = totalLoss / count;
    } else {
        metric.loss = totalLoss / (count * static_cast<double>(outputs));
        metric.score = -metric.loss;
    }
    return metric;
}
#endif

class GenerationWorkerPool {
public:
    explicit GenerationWorkerPool(std::size_t workerCount) : remaining_(0) {
        workers_.reserve(workerCount);
        for (std::size_t worker = 0; worker < workerCount; ++worker) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~GenerationWorkerPool() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
            ++epoch_;
        }
        workReady_.notify_all();
        // Join while the mutex and condition variables are still alive. Since
        // workers_ is declared before them, relying on implicit member
        // destruction would otherwise destroy the synchronization state first.
        workers_.clear();
    }

    GenerationWorkerPool(const GenerationWorkerPool&) = delete;
    GenerationWorkerPool& operator=(const GenerationWorkerPool&) = delete;

    void run(std::size_t itemCount, std::function<void(std::size_t)> task) {
        {
            std::lock_guard lock(mutex_);
            task_ = std::move(task);
            itemCount_ = itemCount;
            cursor_.store(0, std::memory_order_relaxed);
            remaining_ = workers_.size();
            error_ = nullptr;
            ++epoch_;
        }
        workReady_.notify_all();
        std::unique_lock lock(mutex_);
        completed_.wait(lock, [this] { return remaining_ == 0; });
        task_ = {};
        if (error_) std::rethrow_exception(error_);
    }

private:
    void workerLoop() {
        std::size_t observedEpoch = 0;
        while (true) {
            std::function<void(std::size_t)> task;
            std::size_t itemCount = 0;
            {
                std::unique_lock lock(mutex_);
                workReady_.wait(lock, [this, &observedEpoch] {
                    return stopping_ || epoch_ != observedEpoch;
                });
                if (stopping_) return;
                observedEpoch = epoch_;
                task = task_;
                itemCount = itemCount_;
            }
            while (true) {
                const std::size_t index = cursor_.fetch_add(1, std::memory_order_relaxed);
                if (index >= itemCount) break;
                try {
                    task(index);
                } catch (...) {
                    cursor_.store(itemCount, std::memory_order_relaxed);
                    std::lock_guard lock(mutex_);
                    if (!error_) error_ = std::current_exception();
                }
            }
            {
                std::lock_guard lock(mutex_);
                if (--remaining_ == 0) completed_.notify_one();
            }
        }
    }

    std::vector<std::jthread> workers_;
    std::atomic<std::size_t> cursor_{0};
    std::mutex mutex_;
    std::condition_variable workReady_;
    std::condition_variable completed_;
    std::function<void(std::size_t)> task_;
    std::exception_ptr error_;
    std::size_t itemCount_ = 0;
    std::size_t remaining_;
    std::size_t epoch_ = 0;
    bool stopping_ = false;
};

#if defined(NEUROEVO_USE_ACCELERATE)
struct EvaluationBatch {
    std::vector<std::size_t> indices;
    std::size_t estimatedCost = 0;
};

std::vector<std::size_t> topologyKey(const Genome& genome) {
    std::vector<std::size_t> key;
    key.reserve(genome.layers.size() * 2 + 1);
    key.push_back(genome.layers.size());
    for (const auto& layer : genome.layers) {
        key.push_back(layer.inputs);
        key.push_back(layer.outputs);
    }
    return key;
}

std::size_t evaluationCost(const Genome& genome, std::size_t sampleCount) {
    std::size_t operations = 0;
    for (const auto& layer : genome.layers) operations += layer.inputs * layer.outputs;
    return operations * sampleCount;
}

std::vector<EvaluationBatch> makeEvaluationBatches(const std::vector<Genome>& population,
                                                   std::size_t workerCount,
                                                   std::size_t sampleCount) {
    std::map<std::vector<std::size_t>, std::vector<std::size_t>> groups;
    for (std::size_t index = 0; index < population.size(); ++index) {
        groups[topologyKey(population[index])].push_back(index);
    }

    std::vector<EvaluationBatch> batches;
    for (auto& [key, indices] : groups) {
        static_cast<void>(key);
        const std::size_t chunkSize = std::max<std::size_t>(
            1, (indices.size() + workerCount * 2 - 1) / (workerCount * 2));
        for (std::size_t offset = 0; offset < indices.size(); offset += chunkSize) {
            const std::size_t count = std::min(chunkSize, indices.size() - offset);
            EvaluationBatch batch;
            batch.indices.insert(
                batch.indices.end(),
                indices.begin() + static_cast<std::ptrdiff_t>(offset),
                indices.begin() + static_cast<std::ptrdiff_t>(offset + count));
            batch.estimatedCost =
                evaluationCost(population[batch.indices.front()], sampleCount) * count;
            batches.push_back(std::move(batch));
        }
    }
    std::stable_sort(batches.begin(), batches.end(), [](const auto& left, const auto& right) {
        return left.estimatedCost > right.estimatedCost;
    });
    return batches;
}
#endif

bool sameShape(const Genome& a, const Genome& b) {
    if (a.layers.size() != b.layers.size()) return false;
    for (std::size_t i = 0; i < a.layers.size(); ++i) {
        if (a.layers[i].inputs != b.layers[i].inputs ||
            a.layers[i].outputs != b.layers[i].outputs) return false;
    }
    return true;
}

Genome rebuildGenome(const Genome& original,
                     const std::vector<std::size_t>& hidden,
                     std::mt19937_64& rng) {
    const std::size_t inputs = original.layers.front().inputs;
    const std::size_t outputs = original.layers.back().outputs;
    Genome rebuilt = makeRandomGenome(original.task, inputs, outputs, hidden, rng);
    rebuilt.id = original.id;
    rebuilt.parentA = original.parentA;
    rebuilt.parentB = original.parentB;
    rebuilt.generation = original.generation;

    if (rebuilt.layers.size() == original.layers.size()) {
        for (std::size_t layerIndex = 0; layerIndex < rebuilt.layers.size(); ++layerIndex) {
            auto& destination = rebuilt.layers[layerIndex];
            const auto& source = original.layers[layerIndex];
            destination.activation = source.activation;
            const std::size_t copiedOutputs = std::min(destination.outputs, source.outputs);
            const std::size_t copiedInputs = std::min(destination.inputs, source.inputs);
            for (std::size_t output = 0; output < copiedOutputs; ++output) {
                for (std::size_t input = 0; input < copiedInputs; ++input) {
                    destination.weights[output * destination.inputs + input] =
                        source.weights[output * source.inputs + input];
                }
                destination.biases[output] = source.biases[output];
            }
        }
    }
    return rebuilt;
}

Genome crossover(const Genome& fitter, const Genome& other, std::mt19937_64& rng) {
    Genome child = fitter;
    if (!sameShape(fitter, other)) return child;

    std::bernoulli_distribution chooseOther(0.5);
    for (std::size_t i = 0; i < child.layers.size(); ++i) {
        for (std::size_t j = 0; j < child.layers[i].weights.size(); ++j) {
            if (chooseOther(rng)) child.layers[i].weights[j] = other.layers[i].weights[j];
        }
        for (std::size_t j = 0; j < child.layers[i].biases.size(); ++j) {
            if (chooseOther(rng)) child.layers[i].biases[j] = other.layers[i].biases[j];
        }
        if (chooseOther(rng)) child.layers[i].activation = other.layers[i].activation;
    }
    return child;
}

const Genome& tournament(const std::vector<Genome>& population,
                         std::size_t tournamentSize,
                         std::mt19937_64& rng) {
    std::uniform_int_distribution<std::size_t> pick(0, population.size() - 1);
    const Genome* winner = &population[pick(rng)];
    for (std::size_t i = 1; i < tournamentSize; ++i) {
        const Genome& candidate = population[pick(rng)];
        if (candidate.fitness > winner->fitness) winner = &candidate;
    }
    return *winner;
}

bool betterValidation(const Genome& candidate, const Genome& incumbent) {
    if (candidate.validationScore != incumbent.validationScore) {
        return candidate.validationScore > incumbent.validationScore;
    }
    if (candidate.validationLoss != incumbent.validationLoss) {
        return candidate.validationLoss < incumbent.validationLoss;
    }
    return candidate.parameterCount() < incumbent.parameterCount();
}

void expectToken(std::istream& input, const std::string& expected) {
    std::string actual;
    if (!(input >> actual) || actual != expected) {
        throw std::runtime_error("model parse error: expected '" + expected + "'");
    }
}

template <typename T>
void writeVector(std::ostream& output, const std::string& name, const std::vector<T>& values) {
    output << name << ' ' << values.size();
    for (const auto& value : values) output << ' ' << value;
    output << '\n';
}

template <typename T>
std::vector<T> readVector(std::istream& input, const std::string& name) {
    expectToken(input, name);
    std::size_t size = 0;
    if (!(input >> size)) throw std::runtime_error("model parse error reading " + name + " size");
    std::vector<T> result(size);
    for (T& value : result) {
        if (!(input >> value)) throw std::runtime_error("model parse error reading " + name);
    }
    return result;
}

} // namespace

std::string toString(TaskType value) {
    return value == TaskType::Classification ? "classification" : "regression";
}

std::string toString(Activation value) {
    switch (value) {
        case Activation::Tanh: return "tanh";
        case Activation::Relu: return "relu";
        case Activation::LeakyRelu: return "leaky_relu";
    }
    throw std::runtime_error("unknown activation");
}

TaskType parseTask(const std::string& value) {
    if (value == "classification") return TaskType::Classification;
    if (value == "regression") return TaskType::Regression;
    throw std::invalid_argument("task must be 'classification' or 'regression'");
}

Activation parseActivation(const std::string& value) {
    if (value == "tanh") return Activation::Tanh;
    if (value == "relu") return Activation::Relu;
    if (value == "leaky_relu") return Activation::LeakyRelu;
    throw std::invalid_argument("unknown activation: " + value);
}

Dataset Dataset::loadCsv(const std::filesystem::path& path,
                         TaskType taskValue,
                         std::size_t targetColumns,
                         bool hasHeader,
                         std::uint64_t seed,
                         double trainRatio,
                         double validationRatio) {
    if (targetColumns == 0) throw std::invalid_argument("targetColumns must be positive");
    if (!(trainRatio > 0.0) || !(validationRatio > 0.0) ||
        trainRatio + validationRatio >= 1.0) {
        throw std::invalid_argument("split ratios must leave positive train, validation, and test sets");
    }

    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open CSV: " + path.string());
    std::vector<std::vector<float>> rows;
    std::string line;
    std::size_t lineNumber = 0;
    if (hasHeader && std::getline(input, line)) ++lineNumber;
    std::size_t columns = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        auto row = parseNumericRow(line, lineNumber);
        if (columns == 0) columns = row.size();
        if (row.size() != columns) {
            throw std::runtime_error("inconsistent column count at line " + std::to_string(lineNumber));
        }
        rows.push_back(std::move(row));
    }
    if (rows.size() < 12) throw std::runtime_error("CSV needs at least 12 data rows");
    if (columns <= targetColumns) throw std::runtime_error("CSV has no feature columns");
    if (taskValue == TaskType::Classification && targetColumns != 1) {
        throw std::invalid_argument("classification requires exactly one target column");
    }

    Dataset dataset;
    dataset.task = taskValue;
    dataset.inputSize = columns - targetColumns;

    std::vector<float> labels;
    if (taskValue == TaskType::Classification) {
        for (const auto& row : rows) labels.push_back(row.back());
        std::sort(labels.begin(), labels.end());
        labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
        if (labels.size() < 2) throw std::runtime_error("classification requires at least two classes");
        dataset.classValues = labels;
        dataset.outputSize = labels.size();
    } else {
        dataset.outputSize = targetColumns;
        dataset.targetMean.assign(targetColumns, 0.0F);
        dataset.targetScale.assign(targetColumns, 1.0F);
    }

    std::vector<std::size_t> order(rows.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937_64 rng(seed);
    std::shuffle(order.begin(), order.end(), rng);
    std::size_t trainCount = static_cast<std::size_t>(static_cast<double>(rows.size()) * trainRatio);
    std::size_t validationCount =
        static_cast<std::size_t>(static_cast<double>(rows.size()) * validationRatio);
    trainCount = std::max<std::size_t>(1, trainCount);
    validationCount = std::max<std::size_t>(1, validationCount);
    if (trainCount + validationCount >= rows.size()) {
        validationCount = 1;
        trainCount = rows.size() - 2;
    }

    dataset.featureMean.assign(dataset.inputSize, 0.0F);
    dataset.featureScale.assign(dataset.inputSize, 1.0F);
    for (std::size_t i = 0; i < trainCount; ++i) {
        for (std::size_t feature = 0; feature < dataset.inputSize; ++feature) {
            dataset.featureMean[feature] += rows[order[i]][feature];
        }
    }
    for (float& value : dataset.featureMean) value /= static_cast<float>(trainCount);
    for (std::size_t i = 0; i < trainCount; ++i) {
        for (std::size_t feature = 0; feature < dataset.inputSize; ++feature) {
            const float difference = rows[order[i]][feature] - dataset.featureMean[feature];
            dataset.featureScale[feature] += difference * difference;
        }
    }
    for (float& value : dataset.featureScale) {
        value = std::sqrt(value / static_cast<float>(trainCount));
        if (value < 1e-6F) value = 1.0F;
    }
    if (taskValue == TaskType::Regression) {
        for (std::size_t i = 0; i < trainCount; ++i) {
            for (std::size_t target = 0; target < targetColumns; ++target) {
                dataset.targetMean[target] += rows[order[i]][dataset.inputSize + target];
            }
        }
        for (float& value : dataset.targetMean) value /= static_cast<float>(trainCount);
        std::fill(dataset.targetScale.begin(), dataset.targetScale.end(), 0.0F);
        for (std::size_t i = 0; i < trainCount; ++i) {
            for (std::size_t target = 0; target < targetColumns; ++target) {
                const float difference = rows[order[i]][dataset.inputSize + target] -
                                         dataset.targetMean[target];
                dataset.targetScale[target] += difference * difference;
            }
        }
        for (float& value : dataset.targetScale) {
            value = std::sqrt(value / static_cast<float>(trainCount));
            if (value < 1e-6F) value = 1.0F;
        }
    }

    auto makeSample = [&](const std::vector<float>& row) {
        Sample sample;
        sample.features.resize(dataset.inputSize);
        for (std::size_t i = 0; i < dataset.inputSize; ++i) {
            sample.features[i] = (row[i] - dataset.featureMean[i]) / dataset.featureScale[i];
        }
        if (taskValue == TaskType::Classification) {
            sample.target.assign(dataset.outputSize, 0.0F);
            const auto found = std::lower_bound(labels.begin(), labels.end(), row.back());
            sample.target[static_cast<std::size_t>(found - labels.begin())] = 1.0F;
        } else {
            sample.target.resize(targetColumns);
            for (std::size_t target = 0; target < targetColumns; ++target) {
                sample.target[target] =
                    (row[dataset.inputSize + target] - dataset.targetMean[target]) /
                    dataset.targetScale[target];
            }
        }
        return sample;
    };

    for (std::size_t i = 0; i < order.size(); ++i) {
        Sample sample = makeSample(rows[order[i]]);
        if (i < trainCount) dataset.train.samples.push_back(std::move(sample));
        else if (i < trainCount + validationCount) dataset.validation.samples.push_back(std::move(sample));
        else dataset.test.samples.push_back(std::move(sample));
    }
    return dataset;
}

std::size_t Genome::parameterCount() const {
    std::size_t count = 0;
    for (const auto& layer : layers) count += layer.weights.size() + layer.biases.size();
    return count;
}

std::vector<std::size_t> Genome::hiddenShape() const {
    std::vector<std::size_t> shape;
    if (layers.size() <= 1) return shape;
    shape.reserve(layers.size() - 1);
    for (std::size_t i = 0; i + 1 < layers.size(); ++i) shape.push_back(layers[i].outputs);
    return shape;
}

void Genome::validate() const {
    if (layers.empty()) throw std::runtime_error("genome contains no layers");
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        if (layer.inputs == 0 || layer.outputs == 0) throw std::runtime_error("zero-sized layer");
        if (layer.weights.size() != layer.inputs * layer.outputs ||
            layer.biases.size() != layer.outputs) throw std::runtime_error("invalid layer parameter count");
        if (i > 0 && layer.inputs != layers[i - 1].outputs) {
            throw std::runtime_error("disconnected layer dimensions");
        }
    }
}

Network::Network(const Genome& genome) : task_(genome.task), layers_(genome.layers) {
    genome.validate();
}

std::vector<float> Network::predict(const std::vector<float>& input) const {
    std::vector<float> current;
    std::vector<float> output;
    const std::size_t workspaceSize = maximumLayerWidth(layers_);
    current.reserve(workspaceSize);
    output.reserve(workspaceSize);
    predictWithWorkspace(layers_, task_, input, current, output);
    return current;
}

Metric evaluate(const Genome& genome, const Partition& partition) {
    if (partition.samples.empty()) throw std::invalid_argument("cannot evaluate an empty partition");
    genome.validate();
#if defined(NEUROEVO_USE_ACCELERATE)
    // Amortize BLAS dispatch and use cache-efficient matrix-matrix kernels for datasets.
    if (partition.samples.size() >= 32) {
        return evaluateBatchAccelerated(
            genome, packPartition(partition, genome.layers.front().inputs,
                                  genome.layers.back().outputs), acceleratedWorkspace());
    }
#endif
    std::vector<float> prediction;
    std::vector<float> scratch;
    const std::size_t workspaceSize = maximumLayerWidth(genome.layers);
    prediction.reserve(workspaceSize);
    scratch.reserve(workspaceSize);
    Metric metric;
    std::size_t correct = 0;
    double totalLoss = 0.0;
    for (const auto& sample : partition.samples) {
        predictWithWorkspace(genome.layers, genome.task, sample.features, prediction, scratch);
        if (genome.task == TaskType::Classification) {
            const auto predicted = static_cast<std::size_t>(
                std::max_element(prediction.begin(), prediction.end()) - prediction.begin());
            const auto expected = static_cast<std::size_t>(
                std::max_element(sample.target.begin(), sample.target.end()) - sample.target.begin());
            if (predicted == expected) ++correct;
            totalLoss -= std::log(std::max(kEpsilon, prediction[expected]));
        } else {
            for (std::size_t i = 0; i < prediction.size(); ++i) {
                const double difference = static_cast<double>(prediction[i] - sample.target[i]);
                totalLoss += difference * difference;
            }
        }
    }
    const double count = static_cast<double>(partition.samples.size());
    if (genome.task == TaskType::Classification) {
        metric.score = static_cast<double>(correct) / count;
        metric.loss = totalLoss / count;
    } else {
        metric.loss = totalLoss / (count * static_cast<double>(genome.layers.back().outputs));
        metric.score = -metric.loss;
    }
    return metric;
}

Genome makeRandomGenome(TaskType task,
                        std::size_t inputs,
                        std::size_t outputs,
                        const std::vector<std::size_t>& hidden,
                        std::mt19937_64& rng) {
    if (inputs == 0 || outputs == 0) throw std::invalid_argument("network dimensions must be positive");
    Genome genome;
    genome.task = task;
    std::vector<std::size_t> shape;
    shape.push_back(inputs);
    shape.insert(shape.end(), hidden.begin(), hidden.end());
    shape.push_back(outputs);
    std::uniform_int_distribution<int> activationPick(0, 2);
    for (std::size_t i = 0; i + 1 < shape.size(); ++i) {
        if (shape[i] == 0 || shape[i + 1] == 0) throw std::invalid_argument("hidden width must be positive");
        LayerGene layer;
        layer.inputs = shape[i];
        layer.outputs = shape[i + 1];
        layer.activation = static_cast<Activation>(activationPick(rng));
        layer.weights.resize(layer.inputs * layer.outputs);
        layer.biases.assign(layer.outputs, 0.0F);
        const float limit = std::sqrt(6.0F / static_cast<float>(layer.inputs + layer.outputs));
        std::uniform_real_distribution<float> weight(-limit, limit);
        for (float& value : layer.weights) value = weight(rng);
        genome.layers.push_back(std::move(layer));
    }
    return genome;
}

void mutateGenome(Genome& genome,
                  const EvolutionOptions& options,
                  std::mt19937_64& rng) {
    std::bernoulli_distribution mutateWeight(options.weightMutationRate);
    std::normal_distribution<float> delta(0.0F, static_cast<float>(options.weightMutationSigma));
    for (auto& layer : genome.layers) {
        for (float& value : layer.weights) if (mutateWeight(rng)) value += delta(rng);
        for (float& value : layer.biases) if (mutateWeight(rng)) value += delta(rng);
    }

    std::bernoulli_distribution mutateActivation(options.activationMutationRate);
    std::uniform_int_distribution<int> activationPick(0, 2);
    for (std::size_t i = 0; i + 1 < genome.layers.size(); ++i) {
        if (mutateActivation(rng)) genome.layers[i].activation = static_cast<Activation>(activationPick(rng));
    }

    std::bernoulli_distribution mutateTopology(options.topologyMutationRate);
    if (!mutateTopology(rng)) return;
    auto hidden = genome.hiddenShape();
    std::uniform_int_distribution<int> operation(0, 2);
    int selected = operation(rng);
    if (hidden.empty()) selected = 0;

    if (selected == 0 && hidden.size() < options.maxHiddenLayers) {
        std::uniform_int_distribution<std::size_t> position(0, hidden.size());
        std::uniform_int_distribution<std::size_t> width(2, std::max<std::size_t>(2, options.initialHidden * 2));
        hidden.insert(hidden.begin() + static_cast<std::ptrdiff_t>(position(rng)),
                      std::min(width(rng), options.maxLayerWidth));
    } else if (selected == 1 && !hidden.empty()) {
        std::uniform_int_distribution<std::size_t> layerPick(0, hidden.size() - 1);
        const std::size_t index = layerPick(rng);
        std::uniform_int_distribution<int> direction(0, 1);
        if (direction(rng) == 0 && hidden[index] > 2) --hidden[index];
        else if (hidden[index] < options.maxLayerWidth) ++hidden[index];
    } else if (selected == 2 && !hidden.empty()) {
        std::uniform_int_distribution<std::size_t> layerPick(0, hidden.size() - 1);
        hidden.erase(hidden.begin() + static_cast<std::ptrdiff_t>(layerPick(rng)));
    }
    genome = rebuildGenome(genome, hidden, rng);
}

EvolutionEngine::EvolutionEngine(EvolutionOptions options) : options_(std::move(options)) {
    if (options_.populationSize < 4) throw std::invalid_argument("population must be at least 4");
    if (options_.eliteCount == 0 || options_.eliteCount >= options_.populationSize) {
        throw std::invalid_argument("elite count must be positive and smaller than population");
    }
    if (options_.generations == 0) throw std::invalid_argument("generations must be positive");
    if (options_.threads == 0) options_.threads = std::max(1U, std::thread::hardware_concurrency());
}

EvolutionResult EvolutionEngine::run(const Dataset& dataset,
                                     std::ostream& progress,
                                     const ProgressCallback& callback) {
    const auto started = std::chrono::steady_clock::now();
    std::mt19937_64 rng(options_.seed);
    std::vector<Genome> population;
    population.reserve(options_.populationSize);
    std::uint64_t nextId = 1;
    for (std::size_t i = 0; i < options_.populationSize; ++i) {
        const std::size_t jitter = i % 3;
        std::vector<std::size_t> hidden{std::max<std::size_t>(2, options_.initialHidden + jitter)};
        if (i % 7 == 0) hidden.push_back(std::max<std::size_t>(2, options_.initialHidden / 2));
        Genome genome = makeRandomGenome(dataset.task, dataset.inputSize, dataset.outputSize, hidden, rng);
        genome.id = nextId++;
        population.push_back(std::move(genome));
    }

    EvolutionResult result;
    Genome hallOfFame;
    bool hasWinner = false;
    double bestSeen = -std::numeric_limits<double>::infinity();
    std::size_t staleGenerations = 0;
    const std::size_t workerCount = std::min(options_.threads, population.size());
    GenerationWorkerPool workerPool(workerCount);
#if defined(NEUROEVO_USE_ACCELERATE)
    const bool usePackedTrain = dataset.train.samples.size() >= 32;
    const bool usePackedValidation = dataset.validation.samples.size() >= 32;
    PackedPartition packedTrain;
    PackedPartition packedValidation;
    if (usePackedTrain) {
        packedTrain = packPartition(dataset.train, dataset.inputSize, dataset.outputSize);
    }
    if (usePackedValidation) {
        packedValidation = packPartition(dataset.validation, dataset.inputSize, dataset.outputSize);
    }
#endif

    for (std::size_t generation = 0; generation < options_.generations; ++generation) {
        const auto evaluateGenome = [&](std::size_t index) {
            Genome& genome = population[index];
#if defined(NEUROEVO_USE_ACCELERATE)
            if (usePackedTrain || usePackedValidation) genome.validate();
            const Metric training = usePackedTrain
                ? evaluateBatchAccelerated(genome, packedTrain, acceleratedWorkspace())
                : evaluate(genome, dataset.train);
            const Metric validation = usePackedValidation
                ? evaluateBatchAccelerated(genome, packedValidation, acceleratedWorkspace())
                : evaluate(genome, dataset.validation);
#else
            const Metric training = evaluate(genome, dataset.train);
            const Metric validation = evaluate(genome, dataset.validation);
#endif
            genome.trainScore = training.score;
            genome.trainLoss = training.loss;
            genome.validationScore = validation.score;
            genome.validationLoss = validation.loss;
            const double lossGuidance = genome.task == TaskType::Classification
                ? 0.05 * training.loss : 0.0;
            genome.fitness = training.score - lossGuidance -
                options_.complexityPenalty * static_cast<double>(genome.parameterCount());
        };
#if defined(NEUROEVO_USE_ACCELERATE)
        if (usePackedTrain || usePackedValidation) {
            const auto batches = makeEvaluationBatches(
                population, workerCount,
                dataset.train.samples.size() + dataset.validation.samples.size());
            workerPool.run(batches.size(), [&](std::size_t batchIndex) {
                for (const std::size_t index : batches[batchIndex].indices) {
                    evaluateGenome(index);
                }
            });
        } else {
            workerPool.run(population.size(), evaluateGenome);
        }
#else
        workerPool.run(population.size(), evaluateGenome);
#endif
        result.evaluations += population.size();
        std::sort(population.begin(), population.end(),
                  [](const Genome& a, const Genome& b) { return a.fitness > b.fitness; });

        const auto validationBest = std::max_element(
            population.begin(), population.end(),
            [](const Genome& a, const Genome& b) { return betterValidation(b, a); });
        if (!hasWinner || betterValidation(*validationBest, hallOfFame)) {
            hallOfFame = *validationBest;
            hasWinner = true;
        }
        if (validationBest->validationScore > bestSeen + 1e-12) {
            bestSeen = validationBest->validationScore;
            staleGenerations = 0;
        } else {
            ++staleGenerations;
        }

        const double meanFitness = std::accumulate(
            population.begin(), population.end(), 0.0,
            [](double sum, const Genome& genome) { return sum + genome.fitness; }) /
            static_cast<double>(population.size());
        GenerationStats stats{generation, population.front().trainScore,
                              validationBest->validationScore, meanFitness,
                              validationBest->parameterCount()};
        result.history.push_back(stats);
        progress << "generation=" << generation
                 << " train_best=" << std::fixed << std::setprecision(5) << stats.bestTrainScore
                 << " validation_best=" << stats.bestValidationScore
                 << " mean_fitness=" << stats.meanFitness
                 << " parameters=" << stats.bestParameters << '\n';

        if (callback && !callback(stats, *validationBest)) break;

        if (bestSeen >= options_.targetScore || staleGenerations >= options_.patience ||
            generation + 1 == options_.generations) break;

        std::vector<Genome> next;
        next.reserve(options_.populationSize);
        for (std::size_t i = 0; i < options_.eliteCount; ++i) next.push_back(population[i]);
        std::bernoulli_distribution immigrant(options_.immigrantRate);
        while (next.size() < options_.populationSize) {
            Genome child;
            if (immigrant(rng)) {
                child = makeRandomGenome(dataset.task, dataset.inputSize, dataset.outputSize,
                                         {options_.initialHidden}, rng);
            } else {
                const Genome& a = tournament(population, options_.tournamentSize, rng);
                const Genome& b = tournament(population, options_.tournamentSize, rng);
                const Genome& fitter = a.fitness >= b.fitness ? a : b;
                const Genome& other = a.fitness >= b.fitness ? b : a;
                child = crossover(fitter, other, rng);
                child.parentA = a.id;
                child.parentB = b.id;
                mutateGenome(child, options_, rng);
            }
            child.id = nextId++;
            child.generation = generation + 1;
            child.fitness = 0.0;
            next.push_back(std::move(child));
        }
        population = std::move(next);
    }

    result.winner = hallOfFame;
    result.elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}

void SavedModel::save(const std::filesystem::path& path) const {
    genome.validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write model: " + path.string());
    output << std::setprecision(9);
    output << "NEUROEVO_MODEL 2\n";
    output << "task " << toString(genome.task) << '\n';
    writeVector(output, "means", featureMean);
    writeVector(output, "scales", featureScale);
    writeVector(output, "target_means", targetMean);
    writeVector(output, "target_scales", targetScale);
    writeVector(output, "classes", classValues);
    output << "layers " << genome.layers.size() << '\n';
    for (const auto& layer : genome.layers) {
        output << "layer " << layer.inputs << ' ' << layer.outputs << ' '
               << toString(layer.activation) << '\n';
        writeVector(output, "weights", layer.weights);
        writeVector(output, "biases", layer.biases);
    }
    output << "end\n";
}

SavedModel SavedModel::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open model: " + path.string());
    expectToken(input, "NEUROEVO_MODEL");
    int version = 0;
    if (!(input >> version) || version != 2) throw std::runtime_error("unsupported model version");
    expectToken(input, "task");
    std::string task;
    input >> task;
    SavedModel model;
    model.genome.task = parseTask(task);
    model.featureMean = readVector<float>(input, "means");
    model.featureScale = readVector<float>(input, "scales");
    model.targetMean = readVector<float>(input, "target_means");
    model.targetScale = readVector<float>(input, "target_scales");
    model.classValues = readVector<float>(input, "classes");
    expectToken(input, "layers");
    std::size_t layerCount = 0;
    input >> layerCount;
    for (std::size_t i = 0; i < layerCount; ++i) {
        expectToken(input, "layer");
        LayerGene layer;
        std::string activation;
        input >> layer.inputs >> layer.outputs >> activation;
        layer.activation = parseActivation(activation);
        layer.weights = readVector<float>(input, "weights");
        layer.biases = readVector<float>(input, "biases");
        model.genome.layers.push_back(std::move(layer));
    }
    expectToken(input, "end");
    model.genome.validate();
    if (model.featureMean.size() != model.genome.layers.front().inputs ||
        model.featureScale.size() != model.featureMean.size()) {
        throw std::runtime_error("model normalization dimensions are invalid");
    }
    return model;
}

std::vector<float> SavedModel::predictRaw(std::vector<float> features) const {
    if (features.size() != featureMean.size()) throw std::invalid_argument("feature count mismatch");
    for (std::size_t i = 0; i < features.size(); ++i) {
        features[i] = (features[i] - featureMean[i]) / featureScale[i];
    }
    auto prediction = Network(genome).predict(features);
    if (genome.task == TaskType::Regression) {
        if (targetMean.size() != prediction.size() || targetScale.size() != prediction.size()) {
            throw std::runtime_error("model target normalization dimensions are invalid");
        }
        for (std::size_t i = 0; i < prediction.size(); ++i) {
            prediction[i] = prediction[i] * targetScale[i] + targetMean[i];
        }
    }
    return prediction;
}

std::string backendName() {
#if defined(NEUROEVO_USE_ACCELERATE)
    return "hybrid scalar + Apple Accelerate (optimized CPU vector/matrix kernels)";
#else
    return "portable scalar C++";
#endif
}

} // namespace neuroevo
