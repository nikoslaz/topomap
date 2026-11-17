#include "mapping.hpp"
#include "cost.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>

std::vector<int> identityMapping(std::size_t nRanks) {
    std::vector<int> map;
    for (std::size_t i = 0; i < nRanks; i++) {
        map.push_back(static_cast<int>(i)); 
    }
    return map;
}

std::vector<int> randomMapping(std::size_t nRanks, std::size_t nSlots, unsigned seed) {
    if (nSlots < nRanks) {
        throw std::runtime_error("Not enough slots for the number of ranks");
    }

    std::vector<int> map;                    
    for (std::size_t i = 0; i < nSlots; i++) {
        map.push_back(static_cast<int>(i)); 
    }

    std::mt19937 rng(seed);
    std::shuffle(map.begin(), map.end(), rng);

    map.resize(nRanks);                      
    return map;
}


std::vector<int> greedyMapping(const CommMatrix& comm, const DistMatrix& dist, std::size_t nRanks, std::size_t nSlots) {
    if (nSlots < nRanks) {
        throw std::runtime_error("Not enough slots for the number of ranks");
    }

    std::vector<int> map(nRanks, -1);
    std::vector<bool> slotUsed(nSlots, false);
    if (nRanks == 0) return map;

    // total traffic of each rank, in both directions
    std::vector<long long> total(nRanks);                      
    for (std::size_t r = 0; r < nRanks; r++) {
        long long sum = 0;
        for (std::size_t j = 0; j < nRanks; j++) {
            sum += comm[r][j] + comm[j][r];
        }
        total[r] = sum;
    }

    // Step 1: place the busiest rank on slot 0
    std::size_t first = std::max_element(total.begin(), total.end()) - total.begin();
    map[first] = 0;
    slotUsed[0] = true;

    std::vector<long long> link(nRanks);
    std::vector<long long> c(nSlots);

    // place the remaining nRanks - 1 ranks
    for (std::size_t round = 1; round < nRanks; round++) {

        // which rank next?
        long long bestLink = -1;
        std::size_t next = 0;
        for (std::size_t r = 0; r < nRanks; r++) {
            if (map[r] != -1) continue;                 // already placed
            link[r] = 0;
            for (std::size_t p = 0; p < nRanks; p++) {
                if (map[p] == -1) continue;             // partner not placed yet
                link[r] += comm[r][p] + comm[p][r];
            }
            if (link[r] > bestLink ||
                (link[r] == bestLink && total[r] > total[next])) {
                bestLink = link[r];
                next = r;
            }
        }

        // which slot?
        long long bestCost = LLONG_MAX;
        std::size_t best = 0;
        for (std::size_t s = 0; s < nSlots; s++) {
            if (slotUsed[s]) continue;                  // slot taken
            c[s] = 0;
            for (std::size_t p = 0; p < nRanks; p++) {
                if (map[p] == -1) continue;
                c[s] += (comm[next][p] + comm[p][next]) * dist[s][map[p]];
            }
            if (c[s] < bestCost) {                     
                bestCost = c[s];
                best = s;
            }
        }

        map[next] = static_cast<int>(best);
        slotUsed[best] = true;
    }

    return map;
}

static long long swapDelta(const CommMatrix& comm, const DistMatrix& dist, const std::vector<int>& mapping, std::size_t a, std::size_t b) {
    int sa = mapping[a];
    int sb = mapping[b];

    long long sum = 0;
    for (std::size_t k = 0; k < comm.size(); k++) {
        if (k == a || k == b) {continue;}
        int sk = mapping[k];
        long long w_ak = comm[a][k] + comm[k][a];
        long long w_bk = comm[b][k] + comm[k][b];
        sum += (w_ak - w_bk) * (dist[sb][sk] - dist[sa][sk]);
    }

    return sum;
}

// Original version: recomputes the full O(n^2) cost for every trial swap.
// Kept as a reference for correctness tests and benchmarks.
void improveBySwapsNaive(const CommMatrix& comm, const DistMatrix& dist, std::vector<int>& mapping) {
    long long currentCost = cost(comm, dist, mapping);
    bool improved = true;

    while (improved) {
        improved = false;
        for (std::size_t i = 0; i < mapping.size(); i++) {
            for (std::size_t j = i + 1; j < mapping.size(); j++) {
                std::swap(mapping[i], mapping[j]);
                long long c = cost(comm, dist, mapping);
                if (c < currentCost) {
                    currentCost = c;
                    improved = true;
                } else {
                    std::swap(mapping[i], mapping[j]);
                }
            }
        }
    }
}

// Fast version: computes only the change in cost of a swap, O(n) instead of O(n^2).
void improveBySwaps(const CommMatrix& comm, const DistMatrix& dist, std::vector<int>& mapping) {
    long long currentCost = cost(comm, dist, mapping);
    bool improved = true;

    while (improved) {
        improved = false;
        for (std::size_t i = 0; i < mapping.size(); i++) {
            for (std::size_t j = i + 1; j < mapping.size(); j++) {
                long long d = swapDelta(comm, dist, mapping, i, j);
                if (d < 0) {
                    std::swap(mapping[i], mapping[j]);
                    currentCost += d;
                    improved = true;
                }
            }
        }
    }

    assert(currentCost == cost(comm, dist, mapping));
}

std::vector<int> bestOfRestarts(const CommMatrix& comm, const DistMatrix& dist,
                                unsigned nRestarts, unsigned seed) {
    if (nRestarts == 0) {
        throw std::runtime_error("bestOfRestarts needs at least one restart");
    }

    std::vector<int> best;
    long long bestCost = LLONG_MAX;
    for (unsigned k = 0; k < nRestarts; k++) {
        std::vector<int> m = randomMapping(comm.size(), dist.size(), seed + k);
        improveBySwaps(comm, dist, m);
        long long c = cost(comm, dist, m);
        if (c < bestCost) {
            bestCost = c;
            best = std::move(m);
        }
    }
    return best;
}
