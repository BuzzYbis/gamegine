struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float3 inNormal   : NORMAL;
    [[vk::location(2)]] float2 inUv       : TEXCOORD0;
};

struct UniformBuffer {
    float4x4 viewProj;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]] uniform PushConstants pushConstants;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 fragTexCoord : TEXCOORD0;
    float3 fragNormal : NORMAL;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(pushConstants.model, float4(input.inPosition, 1.0));
    output.pos = mul(ubo.viewProj, output.pos);

    output.fragTexCoord = input.inUv;
    output.fragNormal = input.inNormal;
    return output;
}

[[vk::binding(0, 1)]] Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}
