// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// A probe that claims both targets but calls something its declaration does
// not cover (and Metal lacks). That is a defect in the kernel, not an
// unsupported target, so it fails -- on the SPIR-V build too, which is the
// point of declaring capabilities in the source.
//
//@expect spirv=FAILED msl=FAILED

RWStructuredBuffer<uint> output;

[require(spirv)]
[require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void match(uint3 id : SV_DispatchThreadID)
{
    output[id.x] = WaveMatch(id.x).x;
}
