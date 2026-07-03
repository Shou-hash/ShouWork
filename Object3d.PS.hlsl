#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

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
    
    if (gMaterial.enableLighting != 0)
    {
        // 法線と「光の逆向き」の内積をとる
        float32_t NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        
        // ハーフランバート反射の計算（saturateで0〜1に安全にクランプしてから変換）
        float32_t cos = saturate(NdotL);
        float32_t halfLambert = pow(cos * 0.5f + 0.5f, 2.0f);
        
        // 最終的なディフューズ色の計算
        float32_t4 diffuse = gMaterial.color * textureColor * gDirectionalLight.color * halfLambert * gDirectionalLight.intensity;
        output.color = diffuse;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}