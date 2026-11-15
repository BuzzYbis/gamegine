// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Apple accepts the MSL Slang emits for this kernel, so the escape beside it
// is dead weight and fails the build: an escape Slang no longer needs is the
// first step of a drift back into two hand-written shader sets.
//
//@expect spirv=COMPILED msl=FAILED

RWStructuredBuffer<uint> output;

[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = id.x;
}
