#include "Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    // 法線をワールド空間に変換（回転のみ適用するため3x3にキャストして乗算）
    output.normal = normalize(mul(float32_t4(input.normal, 0.0f), gTransformationMatrix.World).xyz);
    
    return output;
}