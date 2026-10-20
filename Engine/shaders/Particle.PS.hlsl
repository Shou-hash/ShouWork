#include "Particle.hlsli"

// RootParameter[0]: CBV (b0)
ConstantBuffer<Material> gMaterial : register(b0);

// RootParameter[2]: Texture SRV (t0)
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 2値抜き (Binary Alpha)
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    
    // gMaterial.color * textureColor * input.color でインスタンス色を反映
    output.color = gMaterial.color * textureColor * input.color;
    
    return output;
}