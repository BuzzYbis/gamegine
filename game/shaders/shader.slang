struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inUv;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
};
[[vk::binding(0)]] ConstantBuffer<UniformBuffer> ubo;

struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]] uniform PushConstants pushConstants;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float3 fragNormal;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(pushConstants.model, float4(input.inPosition, 1.0));
    output.pos = mul(ubo.view, output.pos);
    output.pos = mul(ubo.proj, output.pos);

    output.fragTexCoord = input.inUv;
    output.fragNormal = input.inNormal;
    return output;
}

[[vk::binding(1)]] Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}