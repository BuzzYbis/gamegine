// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// The cost of 'doubleSided' on the mesh path.
//
// The same clusters as mesh_cluster_dispatch, but every other cluster is
// wound the other way, so it faces away from the viewer. The harness draws
// them twice through this one kernel: single-sided, with back faces culled by
// the rasterizer, and double-sided, with nothing culled and the fragment
// shader flipping the normal of back faces -- which is what a doubleSided
// material does. The ratio of the two is the directive's cost.
//
// Measures: doubleSided cost on the mesh path, as a ratio against
// single-sided (B0, R4's foliage budget).
//
// Bindings, by name, as build/shaders/index.json records them: params.

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

struct ShadedVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

[require(spirv_1_5, spvMeshShadingEXT)]
[require(metal)]
[shader("mesh")]
[numthreads(64, 1, 1)]
[outputtopology("triangle")]
void facingClusterMS(uint lane : SV_GroupThreadID,
                     uint3 groupId : SV_GroupID,
                     OutputVertices<ShadedVertex, 64> vertices,
                     OutputIndices<uint3, 98> triangles)
{
    MeshParams p     = params[0];
    uint       group = groupId.y * p.groupsPerRow + groupId.x;
    if (group >= p.clusterCount) {
        SetMeshOutputCounts(0, 0);  // the last row's tail
        return;
    }
    bool away = (group & 1u) != 0;
    SetMeshOutputCounts(kClusterVertices, kClusterTriangles);

    // A slight tilt per cluster, so the lighting is not uniform.
    float  tilt = float(pcgHash(group) & 255u) / 255.0 - 0.5;
    ShadedVertex v;
    v.position     = clusterVertex(group, lane, p.gridWidth);
    v.normal       = normalize(float3(tilt, 0.25, 1.0));
    vertices[lane] = v;
    for (uint t = lane; t < kClusterTriangles; t += kClusterVertices) {
        triangles[t] = clusterTriangle(t, away);
    }
}

// Two-sided shading: a back face is lit with its normal flipped.
[require(spirv_1_5)]
[require(metal)]
[shader("fragment")]
float4 shadeFS(ShadedVertex v, bool front : SV_IsFrontFace) : SV_Target
{
    float3 n       = normalize(front ? v.normal : -v.normal);
    float3 l       = normalize(float3(0.3, 0.5, 0.8));
    float  diffuse = saturate(dot(n, l));
    return float4(diffuse, diffuse, diffuse, 1.0);
}
