#!/usr/bin/env python3
"""Generate topomap test cases: a 2D stencil on a G x G grid of ranks.

Each rank talks to its up/down/left/right neighbours (like the heat-equation
solver). The topology is N/CORES nodes x CORES cores: distance 1 inside a
node, 10 between nodes.

Usage:
    python3 tools/gen_tests.py                       # 16 ranks -> tests/comm16.txt, topo16.txt
    python3 tools/gen_tests.py OUT_DIR G CORES       # e.g. bench_data 32 32  (1024 ranks)

Writes OUT_DIR/comm<N>.txt and OUT_DIR/topo<N>.txt, where N = G * G.
"""
import os
import sys

out = sys.argv[1] if len(sys.argv) > 1 else "tests"
G = int(sys.argv[2]) if len(sys.argv) > 2 else 4        # grid is G x G ranks
CORES = int(sys.argv[3]) if len(sys.argv) > 3 else 4    # cores per node
N = G * G
if N % CORES != 0:
    sys.exit("G*G must be divisible by CORES")
TRAFFIC = 100    # bytes sent to each neighbour
NEAR, FAR = 1, 10

os.makedirs(out, exist_ok=True)

comm = [[0] * N for _ in range(N)]
for r in range(G):
    for c in range(G):
        i = r * G + c
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            rr, cc = r + dr, c + dc
            if 0 <= rr < G and 0 <= cc < G:
                comm[i][rr * G + cc] = TRAFFIC

topo = [[0 if a == b else (NEAR if a // CORES == b // CORES else FAR)
         for b in range(N)] for a in range(N)]


def write(name, m):
    with open(os.path.join(out, name), "w") as f:
        f.write(f"{len(m)}\n")
        for row in m:
            f.write(" ".join(map(str, row)) + "\n")


write(f"comm{N}.txt", comm)
write(f"topo{N}.txt", topo)
print(f"wrote {out}/comm{N}.txt and {out}/topo{N}.txt ({N} ranks, {N // CORES} nodes x {CORES} cores)")
