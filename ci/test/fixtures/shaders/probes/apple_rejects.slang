// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// Slang accepts this for both targets, but the MSL it emits calls a function
// that does not exist, so Apple's metal compiler rejects it. For a probe that
// is UNSUPPORTED, with Apple's diagnostic recorded: Slang emitting MSL text
// and Apple accepting it are different checks, and only the second proves a
// kernel loads on Metal.
//
//@expect spirv=COMPILED msl=UNSUPPORTED

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

[require(spirv)]
[require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = throughBackend(id.x);
}
