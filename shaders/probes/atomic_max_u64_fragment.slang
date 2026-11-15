// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// 64-bit atomic max from the fragment stage.
//
// 'layers' full-screen triangles are drawn as instances. Every fragment
// folds a 64-bit key -- a hash of pixel and layer above the layer index --
// into its pixel's slot with InterlockedMax, as a visibility buffer written
// from the raster pipeline would. The host recomputes every pixel's maximum.
//
// Measures: 64-bit fragment-stage atomic availability and cost (B0, D6b's
// default of a shared target). The MSL escape beside this file exists for
// the same reason as atomic_max_u64_buffer's.
//
// Bindings, by name, as build/shaders/index.json records them: params, keys.

import common.bench;

struct FragmentParams
{
    uint width;   // render target width, in pixels
    uint height;
    uint layers;  // instances drawn; each covers every pixel once
    uint pad0;
};

StructuredBuffer<FragmentParams> params;
RWStructuredBuffer<uint64_t>     keys;

struct LayerVertex
{
    float4 position : SV_Position;
    nointerpolation uint layer : LAYER;
};

// One triangle covering the whole viewport, once per layer.
[require(spirv_1_5)]
[require(metal)]
[shader("vertex")]
LayerVertex layerVS(uint vertex : SV_VertexID, uint instance : SV_InstanceID)
{
    float2      uv = float2(float((vertex << 1) & 2u), float(vertex & 2u));
    LayerVertex o;
    o.position = float4(uv * 2.0 - 1.0, 0.5, 1.0);
    o.layer    = instance;
    return o;
}

[require(spirv_1_5, spvInt64Atomics)]
[require(metal)]
[shader("fragment")]
float4 atomicMaxFS(LayerVertex v) : SV_Target
{
    FragmentParams p     = params[0];
    uint           pixel = uint(v.position.y) * p.width + uint(v.position.x);
    uint           h     = pcgHash(pixel * p.layers + v.layer);
    uint64_t       key   = (uint64_t(h) << 32) | uint64_t(v.layer);
    InterlockedMax(keys[pixel], key);
    return float4(0.0, 0.0, 0.0, 1.0);
}
