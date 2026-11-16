#include "topo_io.hpp"
#include "cost.hpp"
#include "mapping.hpp"
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

// One row of the results table: name, cost, and improvement over the identity mapping.
void printRow(const std::string& name, long long c, long long baseline) {
    double pct = 100.0 * static_cast<double>(baseline - c) / static_cast<double>(baseline);
    std::cout << std::left << std::setw(28) << name
              << std::right << std::setw(12) << c
              << std::setw(13) << std::fixed << std::setprecision(1) << pct << "%\n";
}

} // namespace

int main(int argc, char* argv[]) {
    // Usage: topomap [comm.txt topo.txt]  (defaults to the files in tests/)
    std::string commPath = std::string(TESTS_DIR) + "/comm.txt";
    std::string topoPath = std::string(TESTS_DIR) + "/topo.txt";
    if (argc == 3) {
        commPath = argv[1];
        topoPath = argv[2];
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [comm.txt topo.txt]\n";
        return 1;
    }

    try {
        CommMatrix comm = readComm(commPath);
        DistMatrix topo = readTopo(topoPath);

        // Every rank needs its own slot, so we need at least as many slots as ranks.
        if (topo.size() < comm.size()) {
            std::cerr << "Not enough slots: " << comm.size() << " ranks but only "
                      << topo.size() << " slots\n";
            return 1;
        }

        const unsigned nSeeds = 10;     // random baseline: mean over this many seeds
        const unsigned nRestarts = 20;  // random restarts for the best-of search

        // Baselines
        auto id = identityMapping(comm.size());
        long long idCost = cost(comm, topo, id);

        long long randomSum = 0;
        for (unsigned seed = 1; seed <= nSeeds; seed++) {
            randomSum += cost(comm, topo, randomMapping(comm.size(), topo.size(), seed));
        }

        // Heuristics
        auto gr = greedyMapping(comm, topo, comm.size(), topo.size());

        auto id2 = id;
        improveBySwaps(comm, topo, id2);

        auto gr2 = gr;
        improveBySwaps(comm, topo, gr2);

        auto best = bestOfRestarts(comm, topo, nRestarts, 1);

        std::cout << comm.size() << " ranks, " << topo.size() << " slots\n\n";
        std::cout << std::left << std::setw(28) << "mapping"
                  << std::right << std::setw(12) << "cost"
                  << std::setw(14) << "vs identity" << "\n";
        std::cout << std::string(54, '-') << "\n";
        printRow("identity", idCost, idCost);
        printRow("random (mean of " + std::to_string(nSeeds) + ")", randomSum / nSeeds, idCost);
        printRow("greedy", cost(comm, topo, gr), idCost);
        printRow("identity + swaps", cost(comm, topo, id2), idCost);
        printRow("greedy + swaps", cost(comm, topo, gr2), idCost);
        printRow("best of " + std::to_string(nRestarts) + " random restarts",
                 cost(comm, topo, best), idCost);

        std::cout << "\nbest mapping (rank -> slot):\n";
        for (std::size_t r = 0; r < best.size(); r++) {
            std::cout << r << "->" << best[r] << " ";
        }
        std::cout << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
