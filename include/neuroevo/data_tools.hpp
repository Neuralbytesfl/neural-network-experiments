#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace neuroevo {

enum class DataPattern {
    Linear,
    Polynomial,
    Sine,
    Xor,
    Circles,
    Clusters,
    Spiral
};

std::string toString(DataPattern pattern);
DataPattern parseDataPattern(const std::string& value);
bool isClassificationPattern(DataPattern pattern);

struct GenerateOptions {
    DataPattern pattern = DataPattern::Linear;
    std::filesystem::path outputPath;
    std::size_t rows = 1000;
    std::size_t inputCount = 2;
    double minimum = -1.0;
    double maximum = 1.0;
    double noise = 0.05;
    std::uint64_t seed = 42;
    bool includeHeader = true;
};

struct GenerateReport {
    std::size_t rowsWritten = 0;
    std::size_t inputCount = 0;
    std::size_t outputCount = 1;
    bool classification = false;
    std::string formula;
};

GenerateReport generateDataset(const GenerateOptions& options);

struct ProfileOptions {
    std::filesystem::path inputPath;
    bool hasHeader = true;
};

struct DataProfile {
    std::size_t rows = 0;
    std::size_t columns = 0;
    std::size_t completeRows = 0;
    std::size_t missingCells = 0;
    std::size_t malformedRows = 0;
    std::size_t duplicateRows = 0;
};

DataProfile profileDataset(const ProfileOptions& options);

struct CleanOptions {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    bool hasHeader = true;
    std::size_t targetColumns = 1;
    bool imputeMissingFeatures = true;
    bool removeDuplicates = true;
    bool dropMalformedRows = true;
    double clipZScore = 0.0; // 0 disables clipping
};

struct CleanReport {
    std::size_t rowsRead = 0;
    std::size_t rowsWritten = 0;
    std::size_t rowsDropped = 0;
    std::size_t missingValuesImputed = 0;
    std::size_t duplicatesRemoved = 0;
    std::size_t valuesClipped = 0;
};

CleanReport cleanDataset(const CleanOptions& options);

} // namespace neuroevo

