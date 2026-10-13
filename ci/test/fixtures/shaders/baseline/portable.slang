// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// A baseline kernel declared for both targets, with two entry points and a
// subgroup operation. It must compile everywhere.
//
//@expect spirv=COMPILED msl=COMPILED

RWStructuredBuffer<uint> output;
StructuredBuffer<uint>   input;

[require(spirv, subgroup_arithmetic)]
[require(metal, subgroup_arithmetic)]
[shader("compute")]
[numthreads(64, 1, 1)]
void prefixSum(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = WavePrefixSum(input[id.x]);
}

[require(spirv)] [require(metal)]
[shader("compute")] [numthreads(64, 1, 1)]
void copy(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = input[id.x];
}
