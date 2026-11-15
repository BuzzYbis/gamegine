// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Inclusive prefix sum over up to 65536 uints, in three dispatches: scan each
// 256-element block, scan the block totals, add each block's offset.
//
// This is the R0 toolchain kernel (r0-architecture.md section 4): one
// non-trivial source, compiled for SPIR-V and MSL and run on both. It
// exercises what a trivial kernel does not -- thread-group memory, barriers
// inside a loop, several entry points sharing resources, and a result the
// host can check to the last bit. The engine needs it anyway: stream
// compaction for culling is a prefix sum.
//
// Bindings, by name, as build/shaders/index.json records them:
//   params, input, output, blockSums.

struct ScanParams
{
    uint count;       // elements to scan, at most 256 * 256
    uint blockCount;  // ceil(count / 256)
    uint pad0;
    uint pad1;
};

StructuredBuffer<ScanParams> params;
StructuredBuffer<uint>       input;
RWStructuredBuffer<uint>     output;
RWStructuredBuffer<uint>     blockSums;

static const uint kBlock = 256;

groupshared uint g_scan[kBlock];

// Inclusive Hillis-Steele scan of one value per thread across the group.
// Every thread must call it: it synchronises the whole group.
uint scanGroup(uint lane, uint value)
{
    g_scan[lane] = value;
    GroupMemoryBarrierWithGroupSync();
    for (uint offset = 1; offset < kBlock; offset <<= 1) {
        uint addend = lane >= offset ? g_scan[lane - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        g_scan[lane] += addend;
        GroupMemoryBarrierWithGroupSync();
    }
    return g_scan[lane];
}

// Pass 1: scan each block, and record its total.
[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(256, 1, 1)]
void scanBlocks(uint3 group : SV_GroupID, uint3 lane : SV_GroupThreadID)
{
    uint count = params[0].count;
    uint i     = group.x * kBlock + lane.x;
    uint sum   = scanGroup(lane.x, i < count ? input[i] : 0);
    if (i < count) {
        output[i] = sum;
    }
    if (lane.x == kBlock - 1) {
        blockSums[group.x] = sum;
    }
}

// Pass 2, one group: turn the block totals into exclusive block offsets.
[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(256, 1, 1)]
void scanBlockSums(uint3 lane : SV_GroupThreadID)
{
    uint blocks = params[0].blockCount;
    uint total  = lane.x < blocks ? blockSums[lane.x] : 0;
    uint sum    = scanGroup(lane.x, total);
    if (lane.x < blocks) {
        blockSums[lane.x] = sum - total;
    }
}

// Pass 3: add each block's offset to its elements.
[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(256, 1, 1)]
void addBlockOffsets(uint3 group : SV_GroupID, uint3 lane : SV_GroupThreadID)
{
    uint i = group.x * kBlock + lane.x;
    if (i < params[0].count) {
        output[i] += blockSums[group.x];
    }
}
