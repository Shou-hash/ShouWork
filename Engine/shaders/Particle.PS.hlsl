#include "Particle.hlsli"

// RootParameter[0]: CBV (b0)
ConstantBuffer<Material> gMaterial : register(b0);

// RootParameter[2]: Texture SRV (t0) に修正
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 2値抜き (Binary Alpha)
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    
    // Particle用マテリアルカラーとテクスチャカラーの乗算
    output.color = gMaterial.color * textureColor;
    
    return output;
}