#include "neuroevo/data_tools.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace neuroevo {
namespace {

struct Cell {
    bool missing = false;
    bool valid = false;
    double value = 0.0;
};

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> cells;
    std::stringstream stream(line);
    std::string cell;
    while (std::getline(stream, cell, ',')) cells.push_back(trim(cell));
    if (!line.empty() && line.back() == ',') cells.emplace_back();
    return cells;
}

Cell parseCell(const std::string& input) {
    const std::string value = trim(input);
    if (value.empty() || value == "NA" || value == "N/A" || value == "null" ||
        value == "NULL" || value == "?" || value == "nan" || value == "NaN") {
        return {.missing = true, .valid = false, .value = 0.0};
    }
    std::size_t consumed = 0;
    try {
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed)) return {};
        return {.missing = false, .valid = true, .value = parsed};
    } catch (const std::exception&) {
        return {};
    }
}

std::string rowKey(const std::vector<std::string>& cells) {
    std::string key;
    for (const auto& cell : cells) {
        key += trim(cell);
        key.push_back('\x1f');
    }
    return key;
}

void validateGenerate(const GenerateOptions& options) {
    if (options.outputPath.empty()) throw std::invalid_argument("generated-data output path is empty");
    if (options.rows < 12) throw std::invalid_argument("generate at least 12 rows");
    if (options.inputCount == 0 || options.inputCount > 64) {
        throw std::invalid_argument("input count must be between 1 and 64");
    }
    if (!(options.minimum < options.maximum)) throw std::invalid_argument("minimum must be less than maximum");
    if (options.noise < 0.0 || !std::isfinite(options.noise)) {
        throw std::invalid_argument("noise must be a finite nonnegative number");
    }
    if ((options.pattern == DataPattern::Xor || options.pattern == DataPattern::Circles ||
         options.pattern == DataPattern::Clusters || options.pattern == DataPattern::Spiral) &&
        options.inputCount != 2) {
        throw std::invalid_argument("the selected classification pattern requires exactly 2 inputs");
    }
}

std::string formulaFor(DataPattern pattern) {
    switch (pattern) {
        case DataPattern::Linear: return "y = Σ(i + 1)·xᵢ + noise";
        case DataPattern::Polynomial: return "y = x₁² + 0.5·x₂ − 0.25·x₃³ + noise";
        case DataPattern::Sine: return "y = sin(x₁) + 0.5·cos(x₂) + noise";
        case DataPattern::Xor: return "label = (x₁ > midpoint) XOR (x₂ > midpoint)";
        case DataPattern::Circles: return "label = distance from center > 0.55·radius";
        case DataPattern::Clusters: return "two Gaussian clusters";
        case DataPattern::Spiral: return "two interleaved noisy spirals";
    }
    return {};
}

} // namespace

std::string toString(DataPattern pattern) {
    switch (pattern) {
        case DataPattern::Linear: return "linear";
        case DataPattern::Polynomial: return "polynomial";
        case DataPattern::Sine: return "sine";
        case DataPattern::Xor: return "xor";
        case DataPattern::Circles: return "circles";
        case DataPattern::Clusters: return "clusters";
        case DataPattern::Spiral: return "spiral";
    }
    throw std::runtime_error("unknown data pattern");
}

DataPattern parseDataPattern(const std::string& value) {
    if (value == "linear") return DataPattern::Linear;
    if (value == "polynomial") return DataPattern::Polynomial;
    if (value == "sine") return DataPattern::Sine;
    if (value == "xor") return DataPattern::Xor;
    if (value == "circles") return DataPattern::Circles;
    if (value == "clusters") return DataPattern::Clusters;
    if (value == "spiral") return DataPattern::Spiral;
    throw std::invalid_argument("unknown data pattern: " + value);
}

bool isClassificationPattern(DataPattern pattern) {
    return pattern == DataPattern::Xor || pattern == DataPattern::Circles ||
           pattern == DataPattern::Clusters || pattern == DataPattern::Spiral;
}

GenerateReport generateDataset(const GenerateOptions& options) {
    validateGenerate(options);
    std::ofstream output(options.outputPath);
    if (!output) throw std::runtime_error("cannot create generated dataset: " + options.outputPath.string());
    output << std::setprecision(10);
    if (options.includeHeader) {
        for (std::size_t i = 0; i < options.inputCount; ++i) {
            if (i) output << ',';
            output << 'x' << (i + 1);
        }
        output << (isClassificationPattern(options.pattern) ? ",label\n" : ",target\n");
    }

    std::mt19937_64 rng(options.seed);
    std::uniform_real_distribution<double> uniform(options.minimum, options.maximum);
    std::normal_distribution<double> gaussian(0.0, options.noise);
    std::bernoulli_distribution classPick(0.5);
    std::bernoulli_distribution labelNoise(std::min(options.noise, 0.5));
    const double midpoint = (options.minimum + options.maximum) * 0.5;
    const double halfRange = (options.maximum - options.minimum) * 0.5;

    for (std::size_t row = 0; row < options.rows; ++row) {
        std::vector<double> inputs(options.inputCount);
        double target = 0.0;
        if (options.pattern == DataPattern::Clusters) {
            const bool label = classPick(rng);
            const double center = label ? 0.45 : -0.45;
            std::normal_distribution<double> cluster(center, 0.18 + options.noise);
            inputs[0] = std::clamp(midpoint + halfRange * cluster(rng), options.minimum, options.maximum);
            inputs[1] = std::clamp(midpoint + halfRange * cluster(rng), options.minimum, options.maximum);
            target = label ? 1.0 : 0.0;
        } else if (options.pattern == DataPattern::Spiral) {
            const bool label = classPick(rng);
            std::uniform_real_distribution<double> turn(0.0, 1.0);
            const double t = turn(rng);
            const double angle = 3.5 * 3.14159265358979323846 * t +
                                 (label ? 3.14159265358979323846 : 0.0);
            const double radius = halfRange * 0.9 * t;
            inputs[0] = midpoint + radius * std::cos(angle) + halfRange * gaussian(rng);
            inputs[1] = midpoint + radius * std::sin(angle) + halfRange * gaussian(rng);
            target = label ? 1.0 : 0.0;
        } else {
            for (double& input : inputs) input = uniform(rng);
            switch (options.pattern) {
                case DataPattern::Linear:
                    for (std::size_t i = 0; i < inputs.size(); ++i) {
                        target += static_cast<double>(i + 1) * inputs[i];
                    }
                    target += gaussian(rng);
                    break;
                case DataPattern::Polynomial:
                    target = inputs[0] * inputs[0];
                    if (inputs.size() > 1) target += 0.5 * inputs[1];
                    if (inputs.size() > 2) target -= 0.25 * inputs[2] * inputs[2] * inputs[2];
                    for (std::size_t i = 3; i < inputs.size(); ++i) target += 0.1 * inputs[i];
                    target += gaussian(rng);
                    break;
                case DataPattern::Sine:
                    target = std::sin(inputs[0]);
                    if (inputs.size() > 1) target += 0.5 * std::cos(inputs[1]);
                    target += gaussian(rng);
                    break;
                case DataPattern::Xor:
                    target = ((inputs[0] > midpoint) != (inputs[1] > midpoint)) ? 1.0 : 0.0;
                    if (labelNoise(rng)) target = 1.0 - target;
                    break;
                case DataPattern::Circles: {
                    const double x = (inputs[0] - midpoint) / halfRange;
                    const double y = (inputs[1] - midpoint) / halfRange;
                    target = std::sqrt(x * x + y * y) > 0.55 ? 1.0 : 0.0;
                    if (labelNoise(rng)) target = 1.0 - target;
                    break;
                }
                case DataPattern::Clusters:
                case DataPattern::Spiral:
                    break;
            }
        }
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            if (i) output << ',';
            output << inputs[i];
        }
        output << ',' << target << '\n';
    }
    return {options.rows, options.inputCount, 1,
            isClassificationPattern(options.pattern), formulaFor(options.pattern)};
}

DataProfile profileDataset(const ProfileOptions& options) {
    std::ifstream input(options.inputPath);
    if (!input) throw std::runtime_error("cannot open dataset: " + options.inputPath.string());
    DataProfile profile;
    std::string line;
    if (options.hasHeader && std::getline(input, line)) {
        profile.columns = splitCsv(line).size();
    }
    std::set<std::string> seen;
    while (std::getline(input, line)) {
        if (trim(line).empty()) continue;
        ++profile.rows;
        const auto cells = splitCsv(line);
        if (profile.columns == 0) profile.columns = cells.size();
        bool malformed = cells.size() != profile.columns;
        for (const auto& raw : cells) {
            const Cell cell = parseCell(raw);
            if (cell.missing) ++profile.missingCells;
            else if (!cell.valid) malformed = true;
        }
        if (malformed) ++profile.malformedRows;
        else if (std::none_of(cells.begin(), cells.end(), [](const std::string& raw) {
                     return parseCell(raw).missing;
                 })) ++profile.completeRows;
        if (!seen.insert(rowKey(cells)).second) ++profile.duplicateRows;
    }
    return profile;
}

CleanReport cleanDataset(const CleanOptions& options) {
    if (options.inputPath.empty() || options.outputPath.empty()) {
        throw std::invalid_argument("cleaning requires input and output paths");
    }
    if (options.inputPath == options.outputPath) {
        throw std::invalid_argument("cleaned output must not overwrite the source dataset");
    }
    std::ifstream input(options.inputPath);
    if (!input) throw std::runtime_error("cannot open dataset: " + options.inputPath.string());
    std::string header;
    std::string line;
    std::size_t columns = 0;
    if (options.hasHeader && std::getline(input, header)) columns = splitCsv(header).size();
    std::vector<std::vector<Cell>> parsedRows;
    CleanReport report;
    while (std::getline(input, line)) {
        if (trim(line).empty()) continue;
        ++report.rowsRead;
        const auto cells = splitCsv(line);
        if (columns == 0) columns = cells.size();
        if (cells.size() != columns) {
            ++report.rowsDropped;
            if (!options.dropMalformedRows) throw std::runtime_error("row has inconsistent column count");
            continue;
        }
        std::vector<Cell> parsed;
        bool malformed = false;
        parsed.reserve(columns);
        for (const auto& raw : cells) {
            Cell cell = parseCell(raw);
            if (!cell.valid && !cell.missing) malformed = true;
            parsed.push_back(cell);
        }
        if (malformed) {
            ++report.rowsDropped;
            if (!options.dropMalformedRows) throw std::runtime_error("dataset contains nonnumeric cells");
            continue;
        }
        parsedRows.push_back(std::move(parsed));
    }
    if (columns <= options.targetColumns || parsedRows.empty()) {
        throw std::runtime_error("no usable rows or feature columns remain");
    }
    const std::size_t featureColumns = columns - options.targetColumns;
    std::vector<double> means(featureColumns, 0.0);
    std::vector<std::size_t> counts(featureColumns, 0);
    for (const auto& row : parsedRows) {
        for (std::size_t column = 0; column < featureColumns; ++column) {
            if (row[column].valid) {
                means[column] += row[column].value;
                ++counts[column];
            }
        }
    }
    for (std::size_t column = 0; column < featureColumns; ++column) {
        if (counts[column] == 0) throw std::runtime_error("a feature column contains no numeric values");
        means[column] /= static_cast<double>(counts[column]);
    }

    std::vector<std::vector<double>> cleanRows;
    for (auto& row : parsedRows) {
        bool drop = false;
        for (std::size_t column = featureColumns; column < columns; ++column) {
            if (row[column].missing) drop = true; // Never invent targets.
        }
        for (std::size_t column = 0; column < featureColumns; ++column) {
            if (row[column].missing) {
                if (options.imputeMissingFeatures) {
                    row[column] = {.missing = false, .valid = true, .value = means[column]};
                    ++report.missingValuesImputed;
                } else {
                    drop = true;
                }
            }
        }
        if (drop) {
            ++report.rowsDropped;
            continue;
        }
        std::vector<double> values;
        values.reserve(columns);
        for (const Cell& cell : row) values.push_back(cell.value);
        cleanRows.push_back(std::move(values));
    }

    if (options.removeDuplicates) {
        std::set<std::vector<double>> unique;
        std::vector<std::vector<double>> deduplicated;
        for (auto& row : cleanRows) {
            if (unique.insert(row).second) deduplicated.push_back(std::move(row));
            else ++report.duplicatesRemoved;
        }
        cleanRows = std::move(deduplicated);
    }

    if (options.clipZScore > 0.0 && !cleanRows.empty()) {
        std::vector<double> cleanMeans(featureColumns, 0.0);
        std::vector<double> deviations(featureColumns, 0.0);
        for (const auto& row : cleanRows) {
            for (std::size_t column = 0; column < featureColumns; ++column) cleanMeans[column] += row[column];
        }
        for (double& value : cleanMeans) value /= static_cast<double>(cleanRows.size());
        for (const auto& row : cleanRows) {
            for (std::size_t column = 0; column < featureColumns; ++column) {
                const double difference = row[column] - cleanMeans[column];
                deviations[column] += difference * difference;
            }
        }
        for (double& value : deviations) value = std::sqrt(value / static_cast<double>(cleanRows.size()));
        for (auto& row : cleanRows) {
            for (std::size_t column = 0; column < featureColumns; ++column) {
                if (deviations[column] <= 1e-12) continue;
                const double low = cleanMeans[column] - options.clipZScore * deviations[column];
                const double high = cleanMeans[column] + options.clipZScore * deviations[column];
                const double clipped = std::clamp(row[column], low, high);
                if (clipped != row[column]) {
                    row[column] = clipped;
                    ++report.valuesClipped;
                }
            }
        }
    }

    std::ofstream output(options.outputPath);
    if (!output) throw std::runtime_error("cannot write cleaned dataset: " + options.outputPath.string());
    output << std::setprecision(10);
    if (options.hasHeader) output << header << '\n';
    for (const auto& row : cleanRows) {
        for (std::size_t column = 0; column < row.size(); ++column) {
            if (column) output << ',';
            output << row[column];
        }
        output << '\n';
    }
    report.rowsWritten = cleanRows.size();
    return report;
}

} // namespace neuroevo
