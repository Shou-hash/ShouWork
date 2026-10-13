#include "Particle.hlsli"

// StructuredBufferでインスタンスごとのTransformationMatrix配列を受け取る (register: t0)
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // InstanceIDを利用して StructuredBuffer の行列と乗算
    output.position = mul(input.position, gTransformationMatrices[instanceId].WVP);
    output.texcoord = input.texcoord;
    
    // 法線をワールド空間に変換（回転のみ適用するため4次元拡張して乗算後xyzを取得）
    output.normal = normalize(mul(float32_t4(input.normal, 0.0f), gTransformationMatrices[instanceId].World).xyz);
    
    return output;
}