// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
//@escape Slang emits atomic_fetch_max_explicit on atomic_ulong, which MSL does not provide (checked at the pinned Slang, vulkan-sdk-1.4.341.0, and at Slang master in 2026-09). MSL's 64-bit max is atomic_max_explicit, which returns nothing.
//
// Hand-written MSL for atomic_max_u64_fragment.slang, for the MSL target
// only; the SPIR-V target stays in Slang (architecture.md, D3). Same entry
// points, same computation, same buffer indices as the Slang kernel. Delete
// it the day Slang's MSL compiles: the build fails until you do.

#include <metal_stdlib>
using namespace metal;

struct FragmentParams {
    uint width;
    uint height;
    uint layers;
    uint pad0;
};

struct LayerVertex {
    float4 position [[position]];
    uint   layer [[flat]];
};

// common/bench.slang's pcgHash, verbatim.
static uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

vertex LayerVertex layerVS(uint vertexId [[vertex_id]],
                           uint instance [[instance_id]])
{
    float2      uv = float2(float((vertexId << 1) & 2u), float(vertexId & 2u));
    LayerVertex o;
    o.position = float4(uv * 2.0 - 1.0, 0.5, 1.0);
    o.layer    = instance;
    return o;
}

fragment float4 atomicMaxFS(LayerVertex v [[stage_in]],
                            device const FragmentParams* params [[buffer(0)]],
                            device atomic_ulong* keys [[buffer(1)]])
{
    FragmentParams p     = params[0];
    uint           pixel = uint(v.position.y) * p.width + uint(v.position.x);
    uint           h     = pcgHash(pixel * p.layers + v.layer);
    ulong          key   = (ulong(h) << 32) | ulong(v.layer);
    atomic_max_explicit(&keys[pixel], key, memory_order_relaxed);
    return float4(0.0, 0.0, 0.0, 1.0);
}
