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
        // 光の向きを逆転させ、法線との内積をとる
        float32_t NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        
        float32_t halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // 輝度と色を補正して最終カラーを計算
        float32_t4 diffuse = gMaterial.color * textureColor * gDirectionalLight.color * halfLambert * gDirectionalLight.intensity;
        output.color = diffuse;
    }
    else
    {
        // ライティングが無効な場合は、素材の色とテクスチャの色を掛け合わせるのみ
        output.color = gMaterial.color * textureColor;
    }
    
    // アルファ値が0の場合は描画をスキップするなどの処理が必要であれば適宜
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}