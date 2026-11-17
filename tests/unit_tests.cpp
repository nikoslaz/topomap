// Simple self-checking tests. Run: ./unit_tests   (exit code 0 = all passed)
#include "cost.hpp"
#include "mapping.hpp"
#include "topo_io.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            failures++;                                                          \
        }                                                                        \
    } while (0)

template <typename F>
static bool throws(F f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

static void writeFile(const std::string& path, const std::string& text) {
    std::ofstream(path) << text;
}

static bool isValidMapping(const std::vector<int>& m, std::size_t nRanks, std::size_t nSlots) {
    if (m.size() != nRanks) return false;
    std::vector<bool> seen(nSlots, false);
    for (int s : m) {
        if (s < 0 || static_cast<std::size_t>(s) >= nSlots || seen[s]) return false;
        seen[s] = true;
    }
    return true;
}

// Random test problem: random traffic (asymmetric), nodes x cores topology.
static void makeProblem(unsigned seed, std::size_t nodes, std::size_t cores,
                        CommMatrix& comm, DistMatrix& dist) {
    std::mt19937 rng(seed);
    std::size_t n = nodes * cores;
    comm.assign(n, std::vector<long long>(n, 0));
    for (std::size_t i = 0; i < n; i++)
        for (std::size_t j = 0; j < n; j++)
            if (i != j && rng() % 3 == 0) comm[i][j] = rng() % 1000;
    dist.assign(n, std::vector<int>(n, 0));
    for (std::size_t a = 0; a < n; a++)
        for (std::size_t b = 0; b < n; b++)
            if (a != b) dist[a][b] = (a / cores == b / cores) ? 1 : 10;
}

int main() {
    // --- cost: the hand-checked 4-rank example (240 vs 400) ---
    CommMatrix comm4 = {{0,10,0,10},{10,0,10,0},{0,10,0,10},{10,0,10,0}};
    DistMatrix dist4 = {{0,1,5,5},{1,0,5,5},{5,5,0,1},{5,5,1,0}};
    CHECK(cost(comm4, dist4, {0,1,2,3}) == 240);
    CHECK(cost(comm4, dist4, {0,2,1,3}) == 400);

    // --- cost: invalid input must throw ---
    CHECK(throws([&] { cost(comm4, dist4, {0,1,2}); }));        // wrong size
    CHECK(throws([&] { cost(comm4, dist4, {0,1,2,4}); }));      // slot out of range
    CHECK(throws([&] { cost(comm4, dist4, {0,1,2,-1}); }));     // negative slot

    // --- more slots than ranks: only the slots used matter ---
    DistMatrix dist6 = {{0,1,5,5,9,9},{1,0,5,5,9,9},{5,5,0,1,9,9},{5,5,1,0,9,9},{9,9,9,9,0,1},{9,9,9,9,1,0}};
    CHECK(cost(comm4, dist6, {0,1,2,3}) == 240);
    CHECK(throws([&] { cost(comm4, dist4, identityMapping(5)); }));

    // --- loaders ---
    writeFile("/tmp/tm_ok_comm.txt", "2\n0 5\n7 0\n");
    writeFile("/tmp/tm_ok_topo.txt", "2\n0 3\n3 0\n");
    CHECK(readComm("/tmp/tm_ok_comm.txt")[1][0] == 7);
    CHECK(readTopo("/tmp/tm_ok_topo.txt")[0][1] == 3);
    writeFile("/tmp/tm_missing.txt", "3\n0 1 2\n1 0 3\n2 3\n");           // missing value
    writeFile("/tmp/tm_extra.txt", "2\n0 1\n1 0\n9\n");                    // extra data
    writeFile("/tmp/tm_neg.txt", "2\n0 -1\n1 0\n");                        // negative
    writeFile("/tmp/tm_asym.txt", "2\n0 1\n2 0\n");                        // asymmetric distance
    writeFile("/tmp/tm_diag.txt", "2\n1 1\n1 0\n");                        // non-zero diagonal
    CHECK(throws([] { readComm("/tmp/tm_missing.txt"); }));
    CHECK(throws([] { readComm("/tmp/tm_extra.txt"); }));
    CHECK(throws([] { readComm("/tmp/tm_neg.txt"); }));
    CHECK(throws([] { readComm("/tmp/does_not_exist.txt"); }));
    CHECK(throws([] { readTopo("/tmp/tm_asym.txt"); }));
    CHECK(throws([] { readTopo("/tmp/tm_diag.txt"); }));
    CHECK(!throws([] { readComm("/tmp/tm_ok_comm.txt"); }));              // asymmetric traffic is fine

    // --- mapping generators produce valid, reproducible mappings ---
    CHECK(isValidMapping(identityMapping(8), 8, 8));
    for (unsigned seed = 1; seed <= 20; seed++) {
        CHECK(isValidMapping(randomMapping(8, 8, seed), 8, 8));
        CHECK(isValidMapping(randomMapping(5, 12, seed), 5, 12));        // more slots than ranks
    }
    CHECK(randomMapping(8, 8, 3) == randomMapping(8, 8, 3));
    CHECK(throws([] { randomMapping(5, 4, 1); }));

    // --- swapDelta correctness: improveBySwaps keeps the cost bookkeeping exact,
    //     and never makes a mapping worse. (The assert inside improveBySwaps checks
    //     the running total against a full recomputation.) ---
    for (unsigned seed = 1; seed <= 30; seed++) {
        CommMatrix comm; DistMatrix dist;
        makeProblem(seed, 3, 4, comm, dist);
        auto m = randomMapping(comm.size(), dist.size(), seed);
        long long before = cost(comm, dist, m);
        improveBySwaps(comm, dist, m);
        CHECK(isValidMapping(m, comm.size(), dist.size()));
        CHECK(cost(comm, dist, m) <= before);
    }

    // --- fast and naive swap search must agree exactly (same visiting order) ---
    for (unsigned seed = 1; seed <= 30; seed++) {
        CommMatrix comm; DistMatrix dist;
        makeProblem(seed, 4, 4, comm, dist);
        auto fast = randomMapping(comm.size(), dist.size(), seed);
        auto slow = fast;
        improveBySwaps(comm, dist, fast);
        improveBySwapsNaive(comm, dist, slow);
        CHECK(fast == slow);
    }

    // --- greedy gives a valid mapping ---
    for (unsigned seed = 1; seed <= 10; seed++) {
        CommMatrix comm; DistMatrix dist;
        makeProblem(seed, 3, 3, comm, dist);
        CHECK(isValidMapping(greedyMapping(comm, dist, comm.size(), dist.size()),
                             comm.size(), dist.size()));
    }
    CHECK(cost(comm4, dist4, greedyMapping(comm4, dist4, 4, 4)) == 240);

    // --- 16-rank stencil: restarts must reach the known optimum 19200 ---
    {
        CommMatrix comm = readComm(std::string(TESTS_DIR) + "/comm16.txt");
        DistMatrix dist = readTopo(std::string(TESTS_DIR) + "/topo16.txt");
        CHECK(cost(comm, dist, identityMapping(16)) == 26400);
        CHECK(cost(comm, dist, greedyMapping(comm, dist, 16, 16)) == 28200);
        auto best = bestOfRestarts(comm, dist, 20, 1);
        CHECK(isValidMapping(best, 16, 16));
        CHECK(cost(comm, dist, best) == 19200);
    }

    if (failures == 0) std::cout << "all tests passed\n";
    else std::cout << failures << " check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
