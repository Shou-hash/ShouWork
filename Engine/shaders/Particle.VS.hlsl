#include "Particle.hlsli"

// StructuredBuffer で ParticleForGPU 配列を受け取る
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // instanceId を使って該当インスタンスの WVP 行列と乗算
    output.position = mul(input.position, gParticle[instanceId].WVP);
    output.texcoord = input.texcoord;
    
    // 法線をワールド空間に変換
    output.normal = normalize(mul(float4(input.normal, 0.0f), gParticle[instanceId].World).xyz);
    
    // インスタンスごとの色を PixelShader に送る
    output.color = gParticle[instanceId].color;
    
    return output;
}