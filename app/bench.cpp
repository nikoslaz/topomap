// Benchmark: naive swap search (full cost recomputation) vs delta swap search.
// Usage: bench comm.txt topo.txt [skipNaiveAbove=300]
// Both versions start from the same random mapping and must reach the same result.
#include "cost.hpp"
#include "mapping.hpp"
#include "topo_io.hpp"
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

using Clock = std::chrono::steady_clock;

template <typename F>
static double seconds(F f) {
    auto t0 = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " comm.txt topo.txt [skipNaiveAbove=300]\n";
        return 1;
    }
    try {
        CommMatrix comm = readComm(argv[1]);
        DistMatrix topo = readTopo(argv[2]);
        std::size_t skipAbove = argc > 3 ? std::stoul(argv[3]) : 300;
        std::size_t n = comm.size();

        auto start = randomMapping(n, topo.size(), 1);
        long long startCost = cost(comm, topo, start);

        auto fast = start;
        double tFast = seconds([&] { improveBySwaps(comm, topo, fast); });

        std::cout << std::setw(6) << n << " ranks | start " << std::setw(10) << startCost
                  << " | delta: " << std::setw(10) << cost(comm, topo, fast) << " in "
                  << std::fixed << std::setprecision(4) << std::setw(9) << tFast << " s";

        if (n <= skipAbove) {
            auto slow = start;
            double tSlow = seconds([&] { improveBySwapsNaive(comm, topo, slow); });
            std::cout << " | naive: " << std::setw(10) << cost(comm, topo, slow) << " in "
                      << std::setw(9) << tSlow << " s"
                      << " | speedup " << std::setprecision(1) << tSlow / tFast << "x"
                      << (slow == fast ? "" : "  MISMATCH!");
        } else {
            std::cout << " | naive: skipped (too slow)";
        }
        std::cout << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
