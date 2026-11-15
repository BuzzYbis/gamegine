// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
//@escape fixture: an escape that Slang no longer needs

#include <metal_stdlib>
using namespace metal;

[[kernel]] void fill(device uint* output [[buffer(0)]],
                     uint3 id [[thread_position_in_grid]])
{
    output[id.x] = id.x;
}
