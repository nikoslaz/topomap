#ifndef TOPOMAP_COST_HPP
#define TOPOMAP_COST_HPP

#include "topo_io.hpp"
#include <vector>

long long cost(const CommMatrix& comm, const DistMatrix& dist, const std::vector<int>& mapping);

#endif