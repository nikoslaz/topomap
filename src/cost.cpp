#include "cost.hpp"
#include <stdexcept>
#include <string>

long long cost(const CommMatrix& comm, const DistMatrix& dist, const std::vector<int>& mapping) {
    if (dist.size() < comm.size()) {
        throw std::runtime_error("Not enough slots: " + std::to_string(comm.size()) +
                                 " ranks but only " + std::to_string(dist.size()) + " slots");
    }

    if (mapping.size() != comm.size()) {
        throw std::runtime_error("Mapping has " + std::to_string(mapping.size()) +
                                 " entries but there are " + std::to_string(comm.size()) + " ranks");
    }

    for (int slot : mapping) {
        if (slot < 0 || slot >= static_cast<int>(dist.size())) {
            throw std::runtime_error("Invalid slot " + std::to_string(slot) + " in mapping");
        }
    }

    long long total = 0;
    for (std::size_t i = 0; i < comm.size(); i++) {
        for (std::size_t j = 0; j < comm.size(); j++){
            total += comm[i][j] * dist[mapping[i]][mapping[j]];
        }
    }

    return total;
}
