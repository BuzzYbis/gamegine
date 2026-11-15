// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// The same kernel as baseline/vulkan_only.slang, as a probe. Its declaration
// says Metal is not supported, and the toolchain records exactly that.
//
//@expect spirv=COMPILED msl=UNSUPPORTED

RWStructuredBuffer<uint> output;

[require(spirv_1_5)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = id.x;
}
