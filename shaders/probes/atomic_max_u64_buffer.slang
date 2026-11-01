// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// 64-bit atomic max on a storage buffer, under controlled contention.
//
// Every thread folds 'iterations' 64-bit keys into 'slotCount' slots with
// InterlockedMax. One slot is maximum contention; one slot per thread is
// none. The key is a 32-bit hash above the thread id -- the shape of a
// visibility key, depth above payload -- so the host can recompute every
// slot's maximum exactly.
//
// Measures: 64-bit buffer atomic max throughput under contention (B0, D6b).
// The MSL escape beside this file exists because Slang emits an atomic MSL
// does not have; see its '//@escape' line.
//
// Bindings, by name, as build/shaders/index.json records them: params, keys.

import common.bench;

struct AtomicParams
{
    uint slotCount;   // slots the keys fold into; 1 is maximum contention
    uint iterations;  // atomics per thread
    uint pad0;
    uint pad1;
};

StructuredBuffer<AtomicParams> params;
RWStructuredBuffer<uint64_t>   keys;

[require(spirv_1_5, spvInt64Atomics)]
[require(metal)]
[shader("compute")]
[numthreads(256, 1, 1)]
void atomicMaxBuffer(uint3 thread : SV_DispatchThreadID)
{
    AtomicParams p = params[0];
    for (uint i = 0; i < p.iterations; ++i) {
        uint     h   = pcgHash(thread.x * p.iterations + i);
        uint64_t key = (uint64_t(h) << 32) | uint64_t(thread.x);
        InterlockedMax(keys[(thread.x + i * 7919u) % p.slotCount], key);
    }
}
