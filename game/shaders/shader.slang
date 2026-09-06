struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float3 inNormal   : NORMAL;
    [[vk::location(2)]] float2 inUv       : TEXCOORD0;
};

struct UniformBuffer {
    float4x4 viewProj;
    float4 cameraPosition;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]] uniform PushConstants pushConstants;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragWorldPos : TEXCOORD0;
    float2 fragTexCoord : TEXCOORD1;
    float3 fragNormal : NORMAL;
};

// ============================================================================
// VERTEX SHADER
// ============================================================================
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;

    // World pos
    float4 worldPos = mul(pushConstants.model, float4(input.inPosition, 1.0));
    output.fragWorldPos = worldPos.xyz;

    // Clip space
    output.pos = mul(ubo.viewProj, worldPos);

    output.fragTexCoord = input.inUv;

    // Transform the normal into world space
    output.fragNormal = mul((float3x3)pushConstants.model, input.inNormal);
    return output;
}

// ============================================================================
// FRAGMENT SHADER
// ============================================================================

[[vk::binding(0, 1)]] Sampler2D baseColorTex;
[[vk::binding(1, 1)]] Sampler2D metallicRoughnessTex;
[[vk::binding(2, 1)]] Sampler2D normalTex;
[[vk::binding(3, 1)]] Sampler2D occlusionTex;
[[vk::binding(4, 1)]] Sampler2D emissiveTex;

struct MaterialParams {
    float4 baseColorFactor;
    float3 emissiveFactor;
    float  metallicFactor;
    float  roughnessFactor;
    float  normalScale;
    float  occlusionStrength;
    float  alphaCutoff;
    float  transmissionFactor;
};
[[vk::binding(5, 1)]] ConstantBuffer<MaterialParams> material;

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom       = PI * denom * denom;

    return num / max(denom, 0.00001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 GetNormalFromMap(VSOutput input) {
    float3 tangentNormal = normalTex.Sample(input.fragTexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= material.normalScale;

    float3 Q1  = ddx(input.fragWorldPos);
    float3 Q2  = ddy(input.fragWorldPos);
    float2 st1 = ddx(input.fragTexCoord);
    float2 st2 = ddy(input.fragTexCoord);

    float3 N   = normalize(input.fragNormal);
    float3 T   = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B   = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    return normalize(mul(tangentNormal, TBN));
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float4 albedoSample = baseColorTex.Sample(vertIn.fragTexCoord);
    float4 baseColor    = albedoSample * material.baseColorFactor;

    if (baseColor.a < material.alphaCutoff) {
        discard;
    }

    float2 mrSample  = metallicRoughnessTex.Sample(vertIn.fragTexCoord).gb;
    float  roughness = mrSample.x * material.roughnessFactor;
    float  metallic  = mrSample.y * material.metallicFactor;
    float  ao        = occlusionTex.Sample(vertIn.fragTexCoord).r;
    float3 emissive  = emissiveTex.Sample(vertIn.fragTexCoord).rgb * material.emissiveFactor;

    float3 N = GetNormalFromMap(vertIn);
    float3 V = normalize(ubo.cameraPosition.xyz - vertIn.fragWorldPos);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);

    float3 lightDir   = normalize(float3(0.5, 1.0, 0.5));
    float3 lightColor = float3(3.0, 3.0, 3.0);

    float3 L = lightDir;
    float3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float3 numerator   = NDF * G * F;
    float  denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular    = numerator / denominator;

    float3 diffuse = kD * baseColor.rgb / PI * (1.0 - material.transmissionFactor);

    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = (diffuse + specular) * lightColor * NdotL;

    float3 ambient = float3(0.03, 0.03, 0.03) * baseColor.rgb * lerp(1.0, ao, material.occlusionStrength) * (1.0 - material.transmissionFactor);
    float3 color   = ambient + Lo + emissive;

    color = color / (color + 1.0);

    float  viewFresnel = FresnelSchlick(max(dot(N, V), 0.0), F0).g;
    float  alpha       = lerp(baseColor.a, viewFresnel, material.transmissionFactor);

    return float4(color, alpha);
}