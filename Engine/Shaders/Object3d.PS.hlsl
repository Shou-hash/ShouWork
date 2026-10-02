#include "Object3d.hlsli"

// cbuffer 構文に変更
cbuffer gMaterial : register(b0)
{
    Material gMaterial;
};

// float32_t 系を標準の float 型に変更
struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

cbuffer gDirectionalLight : register(b1)
{
    DirectionalLight gDirectionalLight;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // === Lighting方式の分岐 ===
    if (gMaterial.enableLighting == 1) // Lambert
    {
        float NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        float lambert = max(NdotL, 0.0f);
        
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * lambert * gDirectionalLight.intensity;
    }
    else if (gMaterial.enableLighting == 2) // Half Lambert
    {
        float NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * halfLambert * gDirectionalLight.intensity;
    }
    else // 0: Lighting なし
    {
        output.color = gMaterial.color * textureColor;
    }
    
    // アルファ値が0の場合は描画をスキップ
    if (textureColor.a == 0.0f)
    {
        discard;
    }
    
    return output;
}