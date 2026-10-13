// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// A baseline kernel whose declaration leaves Metal out. For a probe that
// would be UNSUPPORTED; for a baseline kernel it is a failure, because a
// required kernel must build for every target (gate B0.1).
//
//@expect spirv=COMPILED msl=FAILED

RWStructuredBuffer<uint> output;

[require(spirv)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = id.x;
}
