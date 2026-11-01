// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
//@escape Slang emits atomic_fetch_max_explicit on atomic_ulong, which MSL does not provide (checked at the pinned Slang, vulkan-sdk-1.4.341.0, and at Slang master in 2026-09). MSL's 64-bit max is atomic_max_explicit, which returns nothing.
//
// Hand-written MSL for atomic_max_u64_buffer.slang, for the MSL target only;
// the SPIR-V target stays in Slang (architecture.md, D3). It must compute
// exactly what the Slang kernel computes, with the same entry point and the
// same buffer indices, which the toolchain checks against Slang's
// reflection. Delete it the day Slang's MSL compiles: the build fails until
// you do.

#include <metal_stdlib>
using namespace metal;

struct AtomicParams {
    uint slotCount;
    uint iterations;
    uint pad0;
    uint pad1;
};

// common/bench.slang's pcgHash, verbatim.
static uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

[[kernel]] void atomicMaxBuffer(device const AtomicParams* params [[buffer(0)]],
                                device atomic_ulong* keys [[buffer(1)]],
                                uint3 tid [[thread_position_in_grid]])
{
    AtomicParams p = params[0];
    for (uint i = 0; i < p.iterations; ++i) {
        uint  h   = pcgHash(tid.x * p.iterations + i);
        ulong key = (ulong(h) << 32) | ulong(tid.x);
        atomic_max_explicit(&keys[(tid.x + i * 7919u) % p.slotCount], key,
                            memory_order_relaxed);
    }
}
