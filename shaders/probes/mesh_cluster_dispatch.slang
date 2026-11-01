// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Mesh-shader cluster dispatch throughput.
//
// One mesh thread group per cluster, 64 threads, emitting its 64 vertices and
// 98 triangles (common/bench.slang). Clusters tile the render target, a few
// pixels each, so triangles are small, as they are on a dense virtualized
// mesh. Every group counts itself, so the host can check that every cluster
// dispatched actually ran.
//
// Measures: mesh-shader cluster dispatch throughput, clusters/ms (B0, D3b).
//
// Bindings, by name, as build/shaders/index.json records them:
//   params, counters.

import common.bench;

struct MeshParams
{
    uint clusterCount;  // clusters to draw
    uint gridWidth;     // clusters per row; gridWidth^2 >= clusterCount
    uint groupsPerRow;  // the dispatch's x size; groups are 2D, because
                        // Metal drops mesh grids past 65535 in one dimension
    uint pad0;
};

StructuredBuffer<MeshParams> params;
RWStructuredBuffer<uint>     counters;

struct ClusterVertex
{
    float4 position : SV_Position;
    nointerpolation uint cluster : CLUSTER;
};

[require(spirv_1_5, spvMeshShadingEXT)]
[require(metal)]
[shader("mesh")]
[numthreads(64, 1, 1)]
[outputtopology("triangle")]
void clusterMS(uint lane : SV_GroupThreadID,
               uint3 groupId : SV_GroupID,
               OutputVertices<ClusterVertex, 64> vertices,
               OutputIndices<uint3, 98> triangles)
{
    MeshParams p     = params[0];
    uint       group = groupId.y * p.groupsPerRow + groupId.x;
    if (group >= p.clusterCount) {
        SetMeshOutputCounts(0, 0);  // the last row's tail
        return;
    }
    SetMeshOutputCounts(kClusterVertices, kClusterTriangles);

    ClusterVertex v;
    v.position     = clusterVertex(group, lane, p.gridWidth);
    v.cluster      = group;
    vertices[lane] = v;
    for (uint t = lane; t < kClusterTriangles; t += kClusterVertices) {
        triangles[t] = clusterTriangle(t, false);
    }
    if (lane == 0) {
        InterlockedAdd(counters[0], 1u);
    }
}

[require(spirv_1_5)]
[require(metal)]
[shader("fragment")]
float4 clusterFS(ClusterVertex v) : SV_Target
{
    uint h = pcgHash(v.cluster);
    return float4(float(h & 255u) / 255.0, float((h >> 8) & 255u) / 255.0,
                  1.0, 1.0);
}
