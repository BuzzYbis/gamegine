// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// A module imported by a fixture kernel. It proves imports resolve against
// the source root and that an edit here reaches the kernels importing it
// (their dependency files list it). Never compiled on its own.

module fixture_hash;

public uint fixture_hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    return x;
}
