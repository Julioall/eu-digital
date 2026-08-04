#include "core/adapters/world_model_adapter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace eu_digital;

namespace {

#if defined(_MSC_VER)
#define EU_DIGITAL_NOINLINE __declspec(noinline)
#else
#define EU_DIGITAL_NOINLINE __attribute__((noinline))
#endif

EU_DIGITAL_NOINLINE PredictionAssessment invoke_concrete(
    WorldModelAdapter* adapter, const std::vector<std::string>& context,
    const std::vector<std::string>& candidates) {
    return adapter->predict(context, "benchmark", candidates);
}

EU_DIGITAL_NOINLINE PredictionAssessment invoke_virtual(
    IPredictionPort* port, const std::vector<std::string>& context,
    const std::vector<std::string>& candidates) {
    return port->predict(context, "benchmark", candidates);
}

template <typename Callable>
double measure(std::size_t iterations, Callable&& callable, std::uint64_t& checksum);

#if defined(_WIN32)

class ThreadAffinityGuard {
public:
    ThreadAffinityGuard() {
        DWORD_PTR process_mask = 0;
        DWORD_PTR system_mask = 0;
        if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) ||
            process_mask == 0) {
            throw std::runtime_error("cannot read process affinity mask");
        }
        const DWORD_PTR selected_cpu = process_mask & (~process_mask + 1);
        previous_mask_ = SetThreadAffinityMask(GetCurrentThread(), selected_cpu);
        if (previous_mask_ == 0) {
            throw std::runtime_error("cannot pin benchmark thread");
        }
    }

    ~ThreadAffinityGuard() {
        if (previous_mask_ != 0) {
            SetThreadAffinityMask(GetCurrentThread(), previous_mask_);
        }
    }

    ThreadAffinityGuard(const ThreadAffinityGuard&) = delete;
    ThreadAffinityGuard& operator=(const ThreadAffinityGuard&) = delete;

private:
    DWORD_PTR previous_mask_{0};
};

std::uint64_t sample_clock() {
    ULONG64 cycles = 0;
    if (!QueryThreadCycleTime(GetCurrentThread(), &cycles)) {
        throw std::runtime_error("cannot read thread cycle time");
    }
    return cycles;
}

#else

class ThreadAffinityGuard {};

std::uint64_t sample_clock() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

#endif

template <typename Callable>
double measure(std::size_t iterations, Callable&& callable, std::uint64_t& checksum) {
    const auto start = sample_clock();
    for (std::size_t index = 0; index < iterations; ++index) {
        checksum += callable().prediction_id.size();
    }
    const auto elapsed = sample_clock() - start;
    return static_cast<double>(elapsed) /
           static_cast<double>(iterations);
}

double percentile(std::vector<double> samples, double fraction) {
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(
        fraction * static_cast<double>(samples.size() - 1));
    return samples[index];
}

double median(const std::vector<double>& samples) {
    return percentile(samples, 0.5);
}

struct PairSample {
    double concrete_cost{0.0};
    double virtual_cost{0.0};

    double overhead() const {
        return (virtual_cost / concrete_cost) - 1.0;
    }
};

template <typename ConcreteCallable, typename VirtualCallable>
PairSample measure_pair(std::size_t iterations, bool virtual_first,
                        ConcreteCallable&& concrete_call,
                        VirtualCallable&& virtual_call, std::uint64_t& checksum) {
    PairSample sample;
    if (virtual_first) {
        sample.virtual_cost += measure(iterations, virtual_call, checksum);
        sample.concrete_cost += measure(iterations, concrete_call, checksum);
        sample.concrete_cost += measure(iterations, concrete_call, checksum);
        sample.virtual_cost += measure(iterations, virtual_call, checksum);
    } else {
        sample.concrete_cost += measure(iterations, concrete_call, checksum);
        sample.virtual_cost += measure(iterations, virtual_call, checksum);
        sample.virtual_cost += measure(iterations, virtual_call, checksum);
        sample.concrete_cost += measure(iterations, concrete_call, checksum);
    }
    sample.concrete_cost /= 2.0;
    sample.virtual_cost /= 2.0;
    return sample;
}

}  // namespace

int main() {
    ThreadAffinityGuard affinity_guard;
    auto model = std::make_shared<WorldModel>(WorldModelConfig{}, "benchmark-stream");
    model->observe("idle", "event-1", 1.0);
    model->observe("active", "event-2", 2.0);
    model->observe("idle", "event-3", 3.0);

    WorldModelAdapter adapter(model);
    IPredictionPort* virtual_port = &adapter;
    const std::vector<std::string> context{"idle"};
    const std::vector<std::string> candidates{"idle", "active"};
    constexpr std::size_t iterations = 5000;
    constexpr std::size_t sample_count = 31;

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < 1000; ++index) {
        checksum += invoke_concrete(&adapter, context, candidates).prediction_id.size();
        checksum += invoke_virtual(virtual_port, context, candidates).prediction_id.size();
    }

    std::vector<double> concrete_samples;
    std::vector<double> virtual_samples;
    std::vector<double> overhead_samples;
    concrete_samples.reserve(sample_count);
    virtual_samples.reserve(sample_count);
    overhead_samples.reserve(sample_count);

    const auto concrete_call = [&] {
        return invoke_concrete(&adapter, context, candidates);
    };
    const auto virtual_call = [&] {
        return invoke_virtual(virtual_port, context, candidates);
    };

    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        const auto paired = measure_pair(iterations, sample % 2 != 0,
                                         concrete_call, virtual_call, checksum);
        concrete_samples.push_back(paired.concrete_cost);
        virtual_samples.push_back(paired.virtual_cost);
        overhead_samples.push_back(paired.overhead());
    }

    const double concrete_cost = median(concrete_samples);
    const double virtual_cost = median(virtual_samples);
    const double overhead = median(overhead_samples);
    std::vector<double> absolute_deviations;
    absolute_deviations.reserve(overhead_samples.size());
    for (const double sample : overhead_samples) {
        absolute_deviations.push_back(std::abs(sample - overhead));
    }
    const double median_absolute_deviation = median(absolute_deviations);
    const double p10 = percentile(overhead_samples, 0.1);
    const double p90 = percentile(overhead_samples, 0.9);

    std::cout << "clock="
#if defined(_WIN32)
              << "thread_cycles"
#else
              << "steady_clock_ns"
#endif
              << '\n'
              << "protocol=paired_abba_baab" << '\n'
              << "sample_count=" << sample_count << '\n'
              << "iterations_per_block=" << iterations << '\n'
              << "concrete_cost=" << concrete_cost << '\n'
              << "virtual_cost=" << virtual_cost << '\n'
              << "overhead_percent=" << overhead * 100.0 << '\n'
              << "overhead_mad_percent=" << median_absolute_deviation * 100.0 << '\n'
              << "overhead_p10_percent=" << p10 * 100.0 << '\n'
              << "overhead_p90_percent=" << p90 * 100.0 << '\n'
              << "checksum=" << checksum << '\n';

    return overhead <= 0.01 ? 0 : 1;
}
