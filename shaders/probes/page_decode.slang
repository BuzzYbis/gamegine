// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// GPU attribute decode of streamed geometry pages.
//
// The provisional page format (to be named in the harness's report; not
// frozen): a page is 64, 128 or 256 KiB of 16-byte vertex records, and each
// page has a header with the bounds its positions are quantized against. A
// record is
//
//   x:16 y:16 | z:16 octahedral normal:8+8 | u:half v:half | cluster id:32
//
// On disk each page is compressed with meshoptimizer's vertex codec; the CPU
// decompresses it back to records, and this kernel expands records into
// float attributes. B0 asks for each of those stages to be timed separately.
//
// Measures: the GPU attribute decode stage of page decode throughput at
// 64 / 128 / 256 KiB (B0; the page size frozen at R1).
//
// Bindings, by name, as build/shaders/index.json records them:
//   params, headers, packed, decoded.

struct DecodeParams
{
    uint vertexCount;      // records to decode
    uint verticesPerPage;  // page bytes / 16
    uint pad0;
    uint pad1;
};

struct PageHeader
{
    float4 minimum;  // xyz: the page's position bounds, minimum corner
    float4 extent;   // xyz: bounds size; a quantized 65535 maps to it
};

struct DecodedVertex
{
    float4 positionU;  // xyz position, w texture u
    float4 normalV;    // xyz unit normal, w texture v
};

StructuredBuffer<DecodeParams>  params;
StructuredBuffer<PageHeader>    headers;
StructuredBuffer<uint4>         packed;
RWStructuredBuffer<DecodedVertex> decoded;

// Half to float with integer operations only. f16tof32 would make the
// SPIR-V declare the Int16 and Float16 capabilities, which need
// shaderInt16 and shaderFloat16 on Vulkan -- a requirement Slang's
// capability check does not report, and the validation layer does. The
// result is exact, so a host reference decode can match it bit for bit.
float halfToFloat(uint h)
{
    uint sign     = (h & 0x8000u) << 16;
    uint exponent = (h >> 10) & 0x1fu;
    uint mantissa = h & 0x3ffu;
    if (exponent == 0) {
        float v = float(mantissa) * (1.0 / 16777216.0);  // 2^-24
        return sign != 0 ? -v : v;
    }
    if (exponent == 31) {
        return asfloat(sign | 0x7f800000u | (mantissa << 13));
    }
    return asfloat(sign | ((exponent + 112u) << 23) | (mantissa << 13));
}

// Octahedral normal decode: Cigolle et al., "A Survey of Efficient
// Representations for Independent Unit Vectors", JCGT 2014.
float3 octDecode(float2 e)
{
    float3 v = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) {
        float2 s = float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
        v.xy     = (1.0 - abs(v.yx)) * s;
    }
    return normalize(v);
}

[require(spirv_1_5)]
[require(metal)]
[shader("compute")]
[numthreads(256, 1, 1)]
void decodePage(uint3 thread : SV_DispatchThreadID)
{
    DecodeParams p = params[0];
    uint         i = thread.x;
    if (i >= p.vertexCount) {
        return;
    }
    PageHeader h = headers[i / p.verticesPerPage];
    uint4      r = packed[i];

    float3 q   = float3(float(r.x & 0xffffu), float(r.x >> 16),
                        float(r.y & 0xffffu)) / 65535.0;
    float2 oct = float2(float((r.y >> 16) & 0xffu), float(r.y >> 24))
                 / 255.0 * 2.0 - 1.0;

    DecodedVertex d;
    d.positionU = float4(h.minimum.xyz + q * h.extent.xyz,
                         halfToFloat(r.z & 0xffffu));
    d.normalV   = float4(octDecode(oct), halfToFloat(r.z >> 16));
    decoded[i]  = d;
}
