#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

cbuffer gTransformationMatrix : register(b0)
{
    TransformationMatrix gTransformationMatrix;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    // 法線をワールド空間に変換
    output.normal = normalize(mul(float4(input.normal, 0.0f), gTransformationMatrix.World).xyz);
    
    return output;
}