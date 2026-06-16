#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};
ConstantBuffer<Material> gMaterial : register(b0);

// 追加：平行光源の構造体と定数バッファ
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1); // register(b1)を使用

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ライティングの有効・無効による分岐
    if (gMaterial.enableLighting != 0)
    {
        // ハーフベクトルではなく、資料の平行光源計算（ランバート反射）
        // 光の向きを逆転させ、法線との内積をとる
        float32_t NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        float32_t cos = saturate(NdotL); // 0.0〜1.0にクランプ
        
        // 輝度（cos * intensity）を計算し、色を乗算
        float32_t3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
        
        // 最終的な色の決定
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * lightColor;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        // ライティング無効時は従来通りの計算
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}