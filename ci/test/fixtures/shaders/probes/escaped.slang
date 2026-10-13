// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Apple rejects the MSL Slang emits for this kernel, and escaped.metal
// replaces it. The MSL target is built from the escape; the SPIR-V target
// stays in Slang, and the rejected Slang attempt stays in the record.
//
//@expect spirv=COMPILED msl=COMPILED

RWStructuredBuffer<uint> output;

uint throughBackend(uint x)
{
    __target_switch
    {
    case spirv:
        return x + 1;
    case metal:
        __intrinsic_asm "gamegine_fixture_undefined($0)";
    }
}

[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = throughBackend(id.x);
}
