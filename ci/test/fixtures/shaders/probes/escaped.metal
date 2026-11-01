// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
//@escape fixture: Slang's MSL for escaped.slang calls a function that does not exist
//
// Same entry point and binding as the Slang kernel: 'output' is buffer 0.

#include <metal_stdlib>
using namespace metal;

[[kernel]] void fill(device uint* output [[buffer(0)]],
                     uint3 id [[thread_position_in_grid]])
{
    output[id.x] = id.x + 1;
}
