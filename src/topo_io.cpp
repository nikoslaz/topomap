#include "topo_io.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Reads a file of the form: n, then an n x n matrix of numbers.
template <typename T>
static std::vector<std::vector<T>> readMatrix(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening the file: " + path);
    }

    int n = 0;
    if (!(file >> n) || n < 0) {
        throw std::runtime_error("Error reading matrix size from " + path);
    }

    std::vector<std::vector<T>> matrix(n, std::vector<T>(n, 0));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!(file >> matrix[i][j])) {
                throw std::runtime_error("Error reading element (" + std::to_string(i) + ", " +
                                         std::to_string(j) + ") from " + path);
            }
            if (matrix[i][j] < 0) {
                throw std::runtime_error("Negative value at (" + std::to_string(i) + ", " +
                                         std::to_string(j) + ") in " + path);
            }
        }
    }

    // Anything left after the n x n numbers means the file doesn't match its header.
    T extra;
    if (file >> extra) {
        throw std::runtime_error("Unexpected extra data after the matrix in " + path);
    }

    return matrix;
}

template <typename T>
static void printMatrixImpl(const std::vector<std::vector<T>>& matrix) {
    std::cout << matrix.size() << "\n";
    for (const auto& row : matrix) {
        for (const T& value : row) {
            std::cout << value << " ";
        }
        std::cout << "\n";
    }
}

void printMatrix(const CommMatrix& matrix) { printMatrixImpl(matrix); }
void printMatrix(const DistMatrix& matrix) { printMatrixImpl(matrix); }

CommMatrix readComm(const std::string& path) {
    return readMatrix<long long>(path);
}

DistMatrix readTopo(const std::string& path) {
    DistMatrix topo = readMatrix<int>(path);
    for (size_t i = 0; i < topo.size(); i++) {
        if (topo[i][i] != 0) {
            throw std::runtime_error("Distance from slot " + std::to_string(i) + " to itself must be 0 in " + path);
        }
        for (size_t j = i + 1; j < topo.size(); j++) {
            if (topo[i][j] != topo[j][i]) {
                throw std::runtime_error("Distance matrix is not symmetric at (" + std::to_string(i) + ", " + std::to_string(j) + ") in " + path);
            }
        }
    }
    return topo;
}
