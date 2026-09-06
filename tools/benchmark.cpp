#include "neuroevo/neuroevo.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>

int main() {
    constexpr std::size_t samples = 4096;
    constexpr std::size_t repetitions = 100;
    std::mt19937_64 rng(20260906);
    std::uniform_real_distribution<float> value(-1.0F, 1.0F);
    const auto genome = neuroevo::makeRandomGenome(
        neuroevo::TaskType::Regression, 32, 4, {64, 64}, rng);
    neuroevo::Network network(genome);
    std::vector<std::vector<float>> inputs(samples, std::vector<float>(32));
    for (auto& row : inputs) for (float& item : row) item = value(rng);

    volatile float checksum = 0.0F;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        for (const auto& input : inputs) checksum += network.predict(input).front();
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double inferences = static_cast<double>(samples * repetitions);
    std::cout << "backend=" << neuroevo::backendName() << '\n'
              << "architecture=32x64x64x4 parameters=" << genome.parameterCount() << '\n'
              << "inferences=" << static_cast<std::uint64_t>(inferences)
              << " elapsed_seconds=" << elapsed
              << " inferences_per_second=" << inferences / elapsed << '\n'
              << "checksum=" << checksum << '\n';
}
