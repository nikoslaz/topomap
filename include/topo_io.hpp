#ifndef TOPOMAP_TOPO_IO_HPP
#define TOPOMAP_TOPO_IO_HPP

#include <string>
#include <vector>

using CommMatrix = std::vector<std::vector<long long>>;  // bytes rank i sends to rank j
using DistMatrix = std::vector<std::vector<int>>;        // distance between slots i and j

CommMatrix readComm(const std::string& path);
DistMatrix readTopo(const std::string& path);

void printMatrix(const CommMatrix& matrix);
void printMatrix(const DistMatrix& matrix);

#endif
