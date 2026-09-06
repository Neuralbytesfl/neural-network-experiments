#include "neuroevo/data_tools.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace {

template <typename Operation>
double measure(Operation&& operation) {
    const auto start = std::chrono::steady_clock::now();
    operation();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace

int main() {
    constexpr std::size_t rows = 100'000;
    const auto temp = std::filesystem::temp_directory_path();
    const auto generated = temp / "neuroevo_data_benchmark_generated.csv";
    const auto cleaned = temp / "neuroevo_data_benchmark_cleaned.csv";
    try {
        neuroevo::GenerateOptions generation;
        generation.pattern = neuroevo::DataPattern::Spiral;
        generation.outputPath = generated;
        generation.rows = rows;
        generation.seed = 20260906;
        const double generateSeconds = measure([&] { neuroevo::generateDataset(generation); });

        neuroevo::DataProfile profile;
        const double profileSeconds = measure([&] {
            profile = neuroevo::profileDataset({generated, true});
        });

        neuroevo::CleanOptions cleaning;
        cleaning.inputPath = generated;
        cleaning.outputPath = cleaned;
        neuroevo::CleanReport report;
        const double cleanSeconds = measure([&] { report = neuroevo::cleanDataset(cleaning); });

        std::cout << "rows=" << rows << " columns=" << profile.columns << '\n'
                  << "generate_seconds=" << generateSeconds
                  << " rows_per_second=" << static_cast<double>(rows) / generateSeconds << '\n'
                  << "profile_seconds=" << profileSeconds
                  << " rows_per_second=" << static_cast<double>(rows) / profileSeconds << '\n'
                  << "clean_seconds=" << cleanSeconds
                  << " rows_per_second=" << static_cast<double>(rows) / cleanSeconds << '\n'
                  << "rows_written=" << report.rowsWritten << '\n';
        std::filesystem::remove(generated);
        std::filesystem::remove(cleaned);
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove(generated);
        std::filesystem::remove(cleaned);
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
