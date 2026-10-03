#include "SPSC-Queue.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace chrono = std::chrono;

constexpr std::size_t CAPACITY = 1 << 14;
constexpr std::size_t TOTAL = 2'000'000;

int main()
{
    SPSCQueue<int> queue(CAPACITY);
    std::atomic<bool> go{false};
    std::size_t consumed = 0;

    std::thread producer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        for (std::size_t i = 0; i < TOTAL; ++i)
            while (!queue.try_emplace(static_cast<int>(i))) {}
    });

    auto t0 = chrono::steady_clock::now();
    go.store(true, std::memory_order_release);

    while (consumed < TOTAL)
    {
        if (queue.try_pop().has_value())
            ++consumed;
    }

    auto elapsed = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - t0).count();

    producer.join();

    std::cout << "[Perf] SPSC int throughput:\t"
              << std::fixed << std::setprecision(1)
              << static_cast<double>(TOTAL) / static_cast<double>(elapsed)
              << "M ops/sec\n";
    return 0;
}