#include "Object3d.hlsli"

// 個別の Material 構造体定義は削除し、hlsliのものを利用します
ConstantBuffer<Material> gMaterial : register(b0);

// 平行光源の構造体と定数バッファ
struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 資料に基づき、入力頂点のTexcoordをz=0の(3+1)次元同次座標と考えて変換を行う
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // 変換後のUV座標（xy）を使ってテクスチャをサンプリング
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ★【2値抜き (Binary Alpha) 処理】
    // テクスチャのアルファ値が 0.5 以下のピクセルは棄却（Depthの書き込みも行われない）
    if (textureColor.a <= 0.5f)
    {
        discard;
    }
    
    // === Lighting方式の分岐 ===
    if (gMaterial.enableLighting == 1) // Lambert
    {
        float32_t NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        float32_t lambert = max(NdotL, 0.0f);
        
        // RGB（カラー）とAlpha（透明度）の計算を分離する
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * lambert * gDirectionalLight.intensity;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else if (gMaterial.enableLighting == 2) // Half Lambert
    {
        float32_t NdotL = dot(normalize(input.normal), -normalize(gDirectionalLight.direction));
        float32_t halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // RGB（カラー）とAlpha（透明度）の計算を分離する
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * halfLambert * gDirectionalLight.intensity;
        output.color.a = gMaterial.color.a * textureColor.a;
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