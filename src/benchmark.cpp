#include "thread_cache.hpp"

#include <barrier>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace {

inline constexpr int k_thread_count = 16;
inline constexpr int k_iterations_per_thread = 100'000;
inline constexpr std::size_t k_min_alloc_size = 16;
inline constexpr std::size_t k_max_alloc_size = 256;

using clock_type = std::chrono::high_resolution_clock;

[[nodiscard]] double to_milliseconds(clock_type::duration duration) noexcept
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

[[nodiscard]] double operations_per_second(
    std::uint64_t total_operations,
    clock_type::duration duration) noexcept
{
    const double seconds = std::chrono::duration<double>(duration).count();
    if (seconds <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(total_operations) / seconds;
}

struct benchmark_result {
    double total_ms;
    double throughput_ops_per_sec;
};

template <typename AllocFn, typename DeallocFn>
[[nodiscard]] benchmark_result run_benchmark(
    AllocFn allocate,
    DeallocFn deallocate,
    const char* label)
{
    std::barrier start_barrier(k_thread_count + 1);
    std::vector<std::jthread> workers;
    workers.reserve(k_thread_count);

    for (int thread_id = 0; thread_id < k_thread_count; ++thread_id) {
        workers.emplace_back([&, thread_id](std::stop_token) {
            start_barrier.arrive_and_wait();

            std::mt19937 rng(static_cast<std::uint32_t>(42u + thread_id));
            std::uniform_int_distribution<std::size_t> size_dist(
                k_min_alloc_size,
                k_max_alloc_size);

            for (int i = 0; i < k_iterations_per_thread; ++i) {
                const std::size_t size = size_dist(rng);
                void* const ptr = allocate(size);

                // Force the compiler to physically map and write to the memory
                *static_cast<volatile char*>(ptr) = 'X';

                deallocate(ptr, size);
            }
        });
    }

    start_barrier.arrive_and_wait();
    const auto t0 = clock_type::now();

    workers.clear();

    const auto t1 = clock_type::now();
    const auto elapsed = t1 - t0;

    const std::uint64_t total_operations =
        static_cast<std::uint64_t>(k_thread_count)
        * static_cast<std::uint64_t>(k_iterations_per_thread)
        * 2u;

    std::cout << "Completed " << label << " benchmark.\n";

    return benchmark_result{
        to_milliseconds(elapsed),
        operations_per_second(total_operations, elapsed),
    };
}

void print_report(
    const benchmark_result& mae,
    const benchmark_result& baseline) noexcept
{
    const std::uint64_t total_operations =
        static_cast<std::uint64_t>(k_thread_count)
        * static_cast<std::uint64_t>(k_iterations_per_thread)
        * 2u;

    const double latency_reduction =
        ((baseline.total_ms - mae.total_ms) / baseline.total_ms) * 100.0;

    std::cout << '\n';
    std::cout << "========================================\n";
    std::cout << " Memory Allocation Engine Benchmark\n";
    std::cout << "========================================\n";
    std::cout << "Threads:            " << k_thread_count << '\n';
    std::cout << "Iterations/thread:  " << k_iterations_per_thread
              << " alloc + dealloc pairs\n";
    std::cout << "Size range:         " << k_min_alloc_size << " - "
              << k_max_alloc_size << " bytes\n";
    std::cout << "Total operations:   " << total_operations << '\n';
    std::cout << "----------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);

    std::cout << "MAE Engine:\n";
    std::cout << "  Total time:       " << mae.total_ms << " ms\n";
    std::cout << "  Throughput:       " << (mae.throughput_ops_per_sec / 1'000'000.0)
              << " M ops/sec\n";
    std::cout << '\n';

    std::cout << "glibc malloc:\n";
    std::cout << "  Total time:       " << baseline.total_ms << " ms\n";
    std::cout << "  Throughput:       "
              << (baseline.throughput_ops_per_sec / 1'000'000.0)
              << " M ops/sec\n";
    std::cout << '\n';

    std::cout << "Latency reduction:  " << latency_reduction << " %\n";
    std::cout << "========================================\n";
}

} // namespace

int main()
{
    std::cout << "Warming up allocators (single-threaded)...\n";

    for (std::size_t size = k_min_alloc_size; size <= k_max_alloc_size; size += 16) {
        void* const mae_ptr = mae::allocate_local(size);
        mae::deallocate_local(mae_ptr, size);

        void* const malloc_ptr = std::malloc(size);
        std::free(malloc_ptr);
    }

    std::cout << "Running MAE benchmark...\n";
    const benchmark_result mae_result = run_benchmark(
        [](std::size_t size) -> void* {
            return mae::allocate_local(size);
        },
        [](void* ptr, std::size_t size) {
            mae::deallocate_local(ptr, size);
        },
        "MAE");

    std::cout << "Running glibc malloc benchmark...\n";
    const benchmark_result malloc_result = run_benchmark(
        [](std::size_t size) -> void* {
            return std::malloc(size);
        },
        [](void* ptr, std::size_t) {
            std::free(ptr);
        },
        "malloc");

    print_report(mae_result, malloc_result);
    return 0;
}
