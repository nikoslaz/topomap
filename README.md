# topomap

A small C++20 command-line tool for **topology-aware MPI rank mapping**: it decides which
MPI rank should run on which core so that ranks that communicate a lot end up close together.

## The problem

An MPI job's ranks don't all talk to each other equally, and a cluster's cores aren't all
equally close. Two ranks on the same node communicate fast; ranks on different nodes go over
the network and are much slower. If ranks are placed without looking at either picture,
heavy communication partners can end up on different nodes and the job wastes time on
communication.

topomap takes:

- a **communication matrix** `comm[i][j]`: bytes rank *i* sends to rank *j*
- a **distance matrix** `dist[a][b]`: how far slot (core) *a* is from slot *b*

and searches for a **mapping** `mapping[rank] = slot` that minimizes

```
cost = Σ_i Σ_j  comm[i][j] × dist[ mapping[i] ][ mapping[j] ]
```

Each rank gets its own slot; the cluster may have more slots than ranks.

## Build

Requires CMake ≥ 3.16 and a C++20 compiler.

```bash
mkdir -p build && cd build
cmake ..
make
```

## Run

```bash
./topomap                                          # small 4-rank example in tests/
./topomap ../tests/comm16.txt ../tests/topo16.txt  # 16-rank example
```

The tool prints a table comparing all mapping strategies and the best mapping found.

## Input format

Both files are plain text: the size `n` on the first line, then an `n × n` matrix of
non-negative integers separated by whitespace.

`comm.txt` — 4 ranks in a ring, 10 bytes to each neighbour:

```
4
0 10 0 10
10 0 10 0
0 10 0 10
10 0 10 0
```

`topo.txt` — 2 nodes with 2 cores each (distance 1 inside a node, 5 between nodes):

```
4
0 1 5 5
1 0 5 5
5 5 0 1
5 5 1 0
```

The loader rejects missing or extra values and negative numbers. The distance matrix must
also be symmetric with a zero diagonal, and there must be at least as many slots as ranks.

## Mapping strategies

| Strategy | Idea |
|---|---|
| **identity** | Rank *i* → slot *i*. The default most launchers use; the baseline. |
| **random** | A random permutation of the slots (seeded, so runs are reproducible). |
| **greedy** | Place the busiest rank first, then repeatedly take the unplaced rank that talks most to the placed ones and put it on the free slot closest to its partners. |
| **swap improvement** | Local search: try swapping the slots of every pair of ranks, keep a swap if the cost drops, repeat until no swap helps. Can start from any mapping. |
| **random restarts** | Run swap improvement from many random starting mappings and keep the best. Escapes the local minima a single swap pass gets stuck in. |

## Results

Test case: a 2D 5-point stencil on a 4 × 4 grid of ranks (each rank sends 100 bytes to each
grid neighbour), mapped onto 4 nodes × 4 cores (distance 1 within a node, 10 between nodes).

| Mapping | Cost | vs identity |
|---|---:|---:|
| identity | 26400 | 0.0% |
| random (mean of 10 seeds) | 39180 | −48.4% |
| greedy | 28200 | −6.8% |
| identity + swaps | 26400 | 0.0% |
| greedy + swaps | 24600 | +6.8% |
| **best of 20 random restarts** | **19200** | **+27.3%** |

Observations:

- **Placement matters.** A random placement costs about 48% more than identity on average. (The random rows depend on
  the standard library's `std::shuffle`, so the exact numbers differ between platforms.)
- **Greedy alone is worse than identity here.** It builds a good compact block on the first
  node, but the leftover ranks are spread awkwardly over the remaining nodes.
- **A single swap pass gets stuck.** Identity is already a local minimum: no single swap
  improves it.
- **Random restarts find the 19200 mapping**, which places a 2 × 2 square of the grid on each
  node, cutting the number of grid links that cross nodes from 12 to 8.

On the small 4-rank example every strategy except random reaches the optimum (240).

## Performance

Swap search tries about n²/2 swaps per pass. The naive version recomputes the whole O(n²)
cost for every trial swap. The fast version (`improveBySwaps`) computes only the change in
cost caused by the swap, which takes O(n). Both visit the swaps in the same order and reach
exactly the same mapping (checked in the unit tests).

Same random starting mapping for both versions, 2D stencil on nodes of √n cores, Release
build (`-O2`), one run each on an Apple Mac:

| Ranks | Naive | Delta | Speedup |
|---:|---:|---:|---:|
| 16 | 0.0001 s | 0.0000 s | 3.8x |
| 64 | 0.0217 s | 0.0020 s | 10.7x |
| 256 | 2.08 s | 0.067 s | 31x |
| 1024 | not run (too slow) | 4.30 s | – |

The delta version wins at every size and the gap widens as the problem grows, as expected
from O(n²) → O(n) per trial swap. Reproduce with:

```bash
python3 tools/gen_tests.py bench_data 16 16      # 256 ranks; use 32 32 for 1024
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target bench
./build-release/bench bench_data/comm256.txt bench_data/topo256.txt
```

Timings are from a single run on one machine and will vary.

## Generating test cases

`tools/gen_tests.py` writes stencil test cases of any size:

```bash
python3 tools/gen_tests.py tests               # 16 ranks (tests/comm16.txt, topo16.txt)
python3 tools/gen_tests.py bench_data 32 32    # 1024 ranks, 32 nodes x 32 cores
```

## Tests

```bash
cd build && cmake .. && make && ctest --output-on-failure
```

## Project layout

```
app/main.cpp            command-line driver and results table
app/bench.cpp           timing: naive vs delta swap search
include/, src/
  topo_io.*             reading and validating the input files
  cost.*                cost of a mapping
  mapping.*             identity, random, greedy, swap improvement (naive + delta), restarts
tests/                  example input files and unit_tests.cpp
tools/gen_tests.py      test case generator
```

## Limitations and next steps

- Each pass of the swap search is still O(n³). Caching per-rank traffic to slot-groups, or
  only trying swaps between ranks on different nodes, would speed it up further.
- Output the best mapping as an Open MPI rankfile so it can be used directly with `mpirun`.
- Generate topologies automatically (fat tree, torus) and read real communication matrices
  from MPI profiling tools.
