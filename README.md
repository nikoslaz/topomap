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
| random (mean of 10 seeds) | 38280 | −45.0% |
| greedy | 28200 | −6.8% |
| identity + swaps | 26400 | 0.0% |
| greedy + swaps | 24600 | +6.8% |
| **best of 20 random restarts** | **19200** | **+27.3%** |

Observations:

- **Placement matters.** A random placement costs 45% more than identity on average.
- **Greedy alone is worse than identity here.** It builds a good compact block on the first
  node, but the leftover ranks are spread awkwardly over the remaining nodes.
- **A single swap pass gets stuck.** Identity is already a local minimum: no single swap
  improves it.
- **Random restarts find the 19200 mapping**, which places a 2 × 2 square of the grid on each
  node, cutting the number of grid links that cross nodes from 12 to 8.

On the small 4-rank example every strategy except random reaches the optimum (240).

## Generating test cases

`tools/gen_tests.py` writes the 16-rank stencil test case:

```bash
python3 tools/gen_tests.py tests
```

## Project layout

```
app/main.cpp            command-line driver and results table
include/, src/
  topo_io.*             reading and validating the input files
  cost.*                cost of a mapping
  mapping.*             identity, random, greedy, swap improvement, random restarts
tests/                  example input files
tools/gen_tests.py      test case generator
```

## Limitations and next steps

- The swap pass recomputes the full cost for every swap (O(n²) each). Computing only the
  change in cost would make it fast enough for hundreds or thousands of ranks.
- Output the best mapping as an Open MPI rankfile so it can be used directly with `mpirun`.
- Generate topologies automatically (fat tree, torus) and read real communication matrices
  from MPI profiling tools.
- Unit tests for the loader and cost function.
