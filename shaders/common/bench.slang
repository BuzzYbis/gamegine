// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Shared by the probe kernels: the hash every probe derives its data from,
// and the procedural clusters the mesh-shader probes draw.
//
// A harness that checks a probe's output recomputes it on the host with the
// same functions, so the host copy must stay identical to this one.

module bench;

// PCG hash: Jarzynski and Olano, "Hash Functions for GPU Rendering", JCGT
// 2020. Cheap, integer-only, and exact on every target.
public uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// A cluster is an 8 x 8 grid of vertices: 64 vertices and 7 x 7 quads, so
// 98 triangles. Clusters tile the viewport in a square grid, one cell each.
// Real clusters are 64 vertices and up to 124 or 126 triangles; this shape
// keeps the vertex count and is exact to generate.
public static const uint kClusterSide      = 8;
public static const uint kClusterVertices  = 64;
public static const uint kClusterTriangles = 98;

// Clip-space position of vertex 'v' of cluster 'cluster', in a grid
// 'gridWidth' clusters wide.
public float4 clusterVertex(uint cluster, uint v, uint gridWidth)
{
    float2 cell  = float2(float(cluster % gridWidth),
                          float(cluster / gridWidth));
    float2 local = float2(float(v % kClusterSide), float(v / kClusterSide))
                   / float(kClusterSide - 1);
    float2 xy    = -1.0 + (cell + local) * (2.0 / float(gridWidth));
    return float4(xy, 0.5, 1.0);
}

// Triangle 't' of a cluster, as indices into its 64 vertices. 'flip'
// reverses the winding, which turns the triangle away from the viewer.
public uint3 clusterTriangle(uint t, bool flip)
{
    uint quad = t / 2;
    uint a    = (quad / (kClusterSide - 1)) * kClusterSide
                + quad % (kClusterSide - 1);
    uint3 tri = (t & 1u) == 0
                    ? uint3(a, a + 1, a + kClusterSide)
                    : uint3(a + 1, a + kClusterSide + 1, a + kClusterSide);
    return flip ? tri.xzy : tri;
}
