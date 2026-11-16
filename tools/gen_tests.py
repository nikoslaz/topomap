#!/usr/bin/env python3
"""Generate a bigger topomap test case.

comm16.txt : 16 ranks on a 4x4 grid, each talks to its up/down/left/right
             neighbours (a 2D stencil, like the heat-equation solver).
topo16.txt : 16 slots = 4 nodes x 4 cores. Distance 1 inside a node,
             10 between nodes.

Usage: python3 tools/gen_tests.py [output_dir]
"""
import sys, os

out = sys.argv[1] if len(sys.argv) > 1 else "tests"
G = 4            # grid is G x G ranks
N = G * G
NODES, CORES = 4, 4
TRAFFIC = 100    # bytes sent to each neighbour
NEAR, FAR = 1, 10

comm = [[0] * N for _ in range(N)]
for r in range(G):
    for c in range(G):
        i = r * G + c
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            rr, cc = r + dr, c + dc
            if 0 <= rr < G and 0 <= cc < G:
                comm[i][rr * G + cc] = TRAFFIC

topo = [[0] * N for _ in range(N)]
for a in range(N):
    for b in range(N):
        if a != b:
            topo[a][b] = NEAR if a // CORES == b // CORES else FAR

def write(name, m):
    with open(os.path.join(out, name), "w") as f:
        f.write(f"{len(m)}\n")
        for row in m:
            f.write(" ".join(map(str, row)) + "\n")

write("comm16.txt", comm)
write("topo16.txt", topo)

# Reference costs for a few mappings (sum over ordered pairs)
def cost(m):
    return sum(comm[i][j] * topo[m[i]][m[j]] for i in range(N) for j in range(N))

identity = list(range(N))
# 2x2 blocks per node: node k holds a 2x2 square of the grid
blocks = []
for br in range(2):
    for bc in range(2):
        for r in range(2):
            for c in range(2):
                blocks.append((br * 2 + r) * G + (bc * 2 + c))
block_map = [0] * N
for slot, rank in enumerate(blocks):
    block_map[rank] = slot
print("identity (one grid row per node):", cost(identity))
print("2x2 blocks per node (good):      ", cost(block_map))
