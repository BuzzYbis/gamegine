// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// A baseline kernel importing a module from common/.
//
//@expect spirv=COMPILED msl=COMPILED

import common.fixture_hash;

RWStructuredBuffer<uint> output;

[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void scramble(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = fixture_hash(id.x);
}
