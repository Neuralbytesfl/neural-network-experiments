#include "neuroevo/neuroevo.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Cli {
    std::filesystem::path data;
    std::filesystem::path output = "winner.neuroevo";
    std::filesystem::path model;
    neuroevo::TaskType task = neuroevo::TaskType::Classification;
    neuroevo::EvolutionOptions evolution;
    std::size_t targetColumns = 1;
    bool header = false;
    bool dryRun = false;
    bool help = false;
    std::string predictRow;
};

void usage(std::ostream& output) {
    output << R"(neuroevo - evolve dense neural networks from numeric CSV data

Training:
  neuroevo --data FILE [options]

Prediction:
  neuroevo --model FILE --predict-row "x1,x2,..."

Required training input:
  --data FILE              Numeric CSV; target column(s) must be last

General options:
  --task TYPE              classification (default) or regression
  --target-columns N       Final target columns; regression may use more than one
  --header                 Skip the first CSV row
  --output FILE            Saved winner (default: winner.neuroevo)
  --seed N                 Reproducible random seed (default: 42)
  --dry-run                Validate input/configuration without evolving
  --help                    Show this help

Evolution options:
  --population N           Candidate count (default: 160)
  --generations N          Generation budget (default: 150)
  --elite N                Unchanged survivors (default: 8)
  --threads N              Evaluation threads (default: hardware concurrency)
  --initial-hidden N       Initial hidden width (default: 6)
  --target-score X         Early-stop validation score (accuracy or negative MSE)
  --patience N             Stop after N non-improving generations (default: 40)
  --weight-rate X          Per-parameter mutation probability (default: 0.12)
  --weight-sigma X         Gaussian mutation scale (default: 0.35)
  --topology-rate X        Architecture mutation probability (default: 0.12)
  --complexity-penalty X   Fitness penalty per parameter (default: 1e-6)

Examples:
  neuroevo --data examples/xor.csv --task classification --seed 7
  neuroevo --model winner.neuroevo --predict-row "0,1"
)";
}

std::string requireValue(int& index, int argc, char** argv, const std::string& option) {
    if (++index >= argc) throw std::invalid_argument(option + " requires a value");
    return argv[index];
}

std::size_t parseSize(const std::string& text, const std::string& option) {
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument(option + " requires a nonnegative integer");
    }
    return value;
}

double parseDouble(const std::string& text, const std::string& option) {
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size()) throw std::invalid_argument(option + " requires a number");
    return value;
}

Cli parseCli(int argc, char** argv) {
    Cli cli;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--help" || option == "-h") cli.help = true;
        else if (option == "--header") cli.header = true;
        else if (option == "--dry-run") cli.dryRun = true;
        else if (option == "--data") cli.data = requireValue(i, argc, argv, option);
        else if (option == "--output") cli.output = requireValue(i, argc, argv, option);
        else if (option == "--model") cli.model = requireValue(i, argc, argv, option);
        else if (option == "--predict-row") cli.predictRow = requireValue(i, argc, argv, option);
        else if (option == "--task") cli.task = neuroevo::parseTask(requireValue(i, argc, argv, option));
        else if (option == "--target-columns") cli.targetColumns = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--population") cli.evolution.populationSize = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--generations") cli.evolution.generations = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--elite") cli.evolution.eliteCount = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--threads") cli.evolution.threads = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--seed") cli.evolution.seed = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--initial-hidden") cli.evolution.initialHidden = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--patience") cli.evolution.patience = parseSize(requireValue(i, argc, argv, option), option);
        else if (option == "--target-score") cli.evolution.targetScore = parseDouble(requireValue(i, argc, argv, option), option);
        else if (option == "--weight-rate") cli.evolution.weightMutationRate = parseDouble(requireValue(i, argc, argv, option), option);
        else if (option == "--weight-sigma") cli.evolution.weightMutationSigma = parseDouble(requireValue(i, argc, argv, option), option);
        else if (option == "--topology-rate") cli.evolution.topologyMutationRate = parseDouble(requireValue(i, argc, argv, option), option);
        else if (option == "--complexity-penalty") cli.evolution.complexityPenalty = parseDouble(requireValue(i, argc, argv, option), option);
        else throw std::invalid_argument("unknown option: " + option);
    }
    return cli;
}

std::vector<float> parseFeatureRow(const std::string& row) {
    std::vector<float> values;
    std::stringstream input(row);
    std::string cell;
    while (std::getline(input, cell, ',')) {
        std::size_t consumed = 0;
        const float value = std::stof(cell, &consumed);
        if (consumed != cell.size()) throw std::invalid_argument("invalid prediction value: " + cell);
        values.push_back(value);
    }
    if (values.empty()) throw std::invalid_argument("prediction row is empty");
    return values;
}

int predict(const Cli& cli) {
    neuroevo::SavedModel model = neuroevo::SavedModel::load(cli.model);
    const auto output = model.predictRaw(parseFeatureRow(cli.predictRow));
    std::cout << std::setprecision(7);
    if (model.genome.task == neuroevo::TaskType::Classification) {
        const std::size_t winner = static_cast<std::size_t>(
            std::max_element(output.begin(), output.end()) - output.begin());
        std::cout << "class=" << model.classValues.at(winner) << " probabilities=";
    } else {
        std::cout << "prediction=";
    }
    for (std::size_t i = 0; i < output.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << output[i];
    }
    std::cout << '\n';
    return 0;
}

int train(const Cli& cli) {
    if (cli.data.empty()) throw std::invalid_argument("--data is required for training");
    const neuroevo::Dataset dataset = neuroevo::Dataset::loadCsv(
        cli.data, cli.task, cli.targetColumns, cli.header, cli.evolution.seed);
    std::cout << "backend=" << neuroevo::backendName() << '\n'
              << "task=" << neuroevo::toString(dataset.task)
              << " inputs=" << dataset.inputSize << " outputs=" << dataset.outputSize
              << " train=" << dataset.train.samples.size()
              << " validation=" << dataset.validation.samples.size()
              << " test=" << dataset.test.samples.size() << '\n';
    if (cli.dryRun) {
        std::cout << "dry_run=passed\n";
        return 0;
    }

    neuroevo::EvolutionEngine engine(cli.evolution);
    neuroevo::EvolutionResult result = engine.run(dataset, std::cout);
    const neuroevo::Metric test = neuroevo::evaluate(result.winner, dataset.test);
    neuroevo::SavedModel{result.winner, dataset.featureMean, dataset.featureScale,
                         dataset.targetMean, dataset.targetScale,
                         dataset.classValues}.save(cli.output);

    std::cout << "winner_validation_score=" << result.winner.validationScore
              << " winner_test_score=" << test.score
              << " test_loss=" << test.loss
              << " parameters=" << result.winner.parameterCount()
              << " evaluations=" << result.evaluations
              << " elapsed_seconds=" << result.elapsedSeconds
              << " evaluations_per_second="
              << static_cast<double>(result.evaluations) / result.elapsedSeconds << '\n'
              << "saved_model=" << cli.output.string() << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Cli cli = parseCli(argc, argv);
        if (cli.help || argc == 1) {
            usage(std::cout);
            return 0;
        }
        if (!cli.model.empty() || !cli.predictRow.empty()) {
            if (cli.model.empty() || cli.predictRow.empty()) {
                throw std::invalid_argument("prediction requires both --model and --predict-row");
            }
            return predict(cli);
        }
        return train(cli);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
