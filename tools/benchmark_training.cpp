#include "neuroevo/neuroevo.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    std::size_t samples = 4096;
    std::size_t repetitions = 8;
    std::size_t population = 64;
    std::size_t threads = 0;
    std::size_t inputs = 16;
    std::size_t outputs = 16;
    std::size_t width = 128;
    std::size_t hiddenLayers = 4;
    bool dryRun = false;
    bool help = false;
};

void usage() {
    std::cout << R"(Usage: neuroevo_training_benchmark [options]

Benchmarks the engine's maximum-topology evaluation path with deterministic,
in-memory classification data. Defaults model the GUI limits: 16 inputs,
16 outputs, and four hidden layers of width 128.

Options:
  --samples N          Samples per partition (default: 4096)
  --repetitions N      Sequential evaluation repetitions (default: 8)
  --population N       Candidates in parallel evaluation (default: 64)
  --threads N          Parallel workers; 0 uses hardware concurrency
  --inputs N           Input features (default: 16)
  --outputs N          Classification outputs (default: 16)
  --width N            Hidden-layer width (default: 128)
  --hidden-layers N    Hidden-layer count (default: 4)
  --dry-run            Validate and print the workload without running it
  --help, -h           Show this help
)";
}

std::size_t parseSize(const std::string& value, const std::string& option, bool allowZero = false) {
    std::size_t parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || (!allowZero && parsed == 0)) {
        throw std::invalid_argument(option + (allowZero ? " requires a nonnegative integer"
                                                       : " requires a positive integer"));
    }
    return parsed;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--help" || option == "-h") {
            options.help = true;
        } else if (option == "--dry-run") {
            options.dryRun = true;
        } else {
            if (i + 1 >= argc) throw std::invalid_argument(option + " requires a value");
            const std::size_t value = parseSize(argv[++i], option, option == "--threads");
            if (option == "--samples") options.samples = value;
            else if (option == "--repetitions") options.repetitions = value;
            else if (option == "--population") options.population = value;
            else if (option == "--threads") options.threads = value;
            else if (option == "--inputs") options.inputs = value;
            else if (option == "--outputs") options.outputs = value;
            else if (option == "--width") options.width = value;
            else if (option == "--hidden-layers") options.hiddenLayers = value;
            else throw std::invalid_argument("unknown option: " + option);
        }
    }
    if (options.threads == 0) {
        options.threads = std::max(1U, std::thread::hardware_concurrency());
    }
    return options;
}

neuroevo::Partition makePartition(const Options& options) {
    std::mt19937_64 rng(20260906);
    std::uniform_real_distribution<float> value(-1.0F, 1.0F);
    neuroevo::Partition partition;
    partition.samples.reserve(options.samples);
    for (std::size_t row = 0; row < options.samples; ++row) {
        neuroevo::Sample sample;
        sample.features.resize(options.inputs);
        for (float& item : sample.features) item = value(rng);
        sample.target.assign(options.outputs, 0.0F);
        sample.target[row % options.outputs] = 1.0F;
        partition.samples.push_back(std::move(sample));
    }
    return partition;
}

double secondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.help) {
            usage();
            return 0;
        }

        const std::vector<std::size_t> hidden(options.hiddenLayers, options.width);
        std::mt19937_64 rng(20260906);
        const auto reference = neuroevo::makeRandomGenome(
            neuroevo::TaskType::Classification, options.inputs, options.outputs, hidden, rng);
        std::cout << "backend=" << neuroevo::backendName() << '\n'
                  << "architecture=" << options.inputs;
        for (std::size_t width : hidden) std::cout << 'x' << width;
        std::cout << 'x' << options.outputs
                  << " parameters=" << reference.parameterCount() << '\n'
                  << "samples=" << options.samples
                  << " repetitions=" << options.repetitions
                  << " population=" << options.population
                  << " threads=" << options.threads << '\n';
        if (options.dryRun) {
            std::cout << "dry_run=passed\n";
            return 0;
        }

        const auto partition = makePartition(options);
        double sequentialChecksum = 0.0;
        const auto sequentialStart = std::chrono::steady_clock::now();
        for (std::size_t repetition = 0; repetition < options.repetitions; ++repetition) {
            const auto metric = neuroevo::evaluate(reference, partition);
            sequentialChecksum += metric.score + metric.loss;
        }
        const double sequentialSeconds = secondsSince(sequentialStart);

        std::vector<neuroevo::Genome> population;
        population.reserve(options.population);
        for (std::size_t i = 0; i < options.population; ++i) {
            population.push_back(neuroevo::makeRandomGenome(
                neuroevo::TaskType::Classification, options.inputs, options.outputs, hidden, rng));
        }
        std::atomic<std::size_t> cursor{0};
        std::vector<double> checksums(options.population, 0.0);
        const std::size_t workers = std::min(options.threads, options.population);
        const auto parallelStart = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for (std::size_t worker = 0; worker < workers; ++worker) {
            threads.emplace_back([&] {
                while (true) {
                    const std::size_t index = cursor.fetch_add(1);
                    if (index >= population.size()) break;
                    const auto metric = neuroevo::evaluate(population[index], partition);
                    checksums[index] = metric.score + metric.loss;
                }
            });
        }
        for (auto& thread : threads) {
            if (thread.joinable()) thread.join();
        }
        const double parallelSeconds = secondsSince(parallelStart);
        double parallelChecksum = 0.0;
        for (double checksum : checksums) parallelChecksum += checksum;

        const double sequentialEvaluations = static_cast<double>(options.repetitions);
        const double parallelEvaluations = static_cast<double>(options.population);
        std::cout << "sequential_seconds=" << sequentialSeconds
                  << " evaluations_per_second=" << sequentialEvaluations / sequentialSeconds
                  << " samples_per_second="
                  << sequentialEvaluations * static_cast<double>(options.samples) / sequentialSeconds
                  << " checksum=" << sequentialChecksum << '\n'
                  << "parallel_seconds=" << parallelSeconds
                  << " evaluations_per_second=" << parallelEvaluations / parallelSeconds
                  << " samples_per_second="
                  << parallelEvaluations * static_cast<double>(options.samples) / parallelSeconds
                  << " checksum=" << parallelChecksum << '\n';
        return std::isfinite(sequentialChecksum) && std::isfinite(parallelChecksum) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
