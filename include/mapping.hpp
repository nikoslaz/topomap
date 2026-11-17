#ifndef TOPOMAP_MAPPING_HPP
#define TOPOMAP_MAPPING_HPP

#include "topo_io.hpp"
#include <cstddef>
#include <vector>

std::vector<int> identityMapping(std::size_t nRanks);
std::vector<int> randomMapping(std::size_t nRanks, std::size_t nSlots, unsigned seed);
std::vector<int> greedyMapping(const CommMatrix& comm, const DistMatrix& dist, std::size_t nRanks, std::size_t nSlots);
// Reference version that recomputes the full cost for every swap
void improveBySwapsNaive(const CommMatrix& comm, const DistMatrix& dist, std::vector<int>& mapping);

// Local search using O(n) cost deltas. Modifies mapping in place.
void improveBySwaps(const CommMatrix& comm, const DistMatrix& dist, std::vector<int>& mapping);

// Runs improveBySwaps from nRestarts random starting mappings (seeds seed, seed+1, ...)
// and returns the cheapest result.
std::vector<int> bestOfRestarts(const CommMatrix& comm, const DistMatrix& dist, unsigned nRestarts, unsigned seed);

#endif