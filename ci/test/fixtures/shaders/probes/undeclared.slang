// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// An entry point with no capability declaration fails on every target, even
// though its body is portable: without the declaration nothing is checked.
// The commented-out attribute below must not count as one.
//
//@expect spirv=FAILED msl=FAILED

RWStructuredBuffer<uint> output;

// [require(spirv)] [require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = id.x;
}
