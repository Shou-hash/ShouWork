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

float HashPS(float2 p)
{
    p = frac(p * float2(127.1f, 311.7f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

float NoisePS(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float a = HashPS(i);
    float b = HashPS(i + float2(1.0f, 0.0f));
    float c = HashPS(i + float2(0.0f, 1.0f));
    float d = HashPS(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// 砂煙用の多層フラクタルノイズ
float FbmDust(float2 uv, float seed)
{
    float2 p = uv * 3.2f + float2(seed * 37.0f, seed * 19.0f);
    float v = 0.0f;
    float amp = 0.55f;
    for (int i = 0; i < 3; ++i)
    {
        v += amp * NoisePS(p);
        p = p * 2.05f + float2(5.2f, 1.3f);
        amp *= 0.5f;
    }
    return v;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // モード0: 従来どおりテクスチャ(circle.png)の2値抜き描画
    if (input.mode < 0.5f)
    {
        if (textureColor.a <= 0.5f)
        {
            discard;
        }
        output.color = gMaterial.color * textureColor * input.color;
        return output;
    }

    // モード1~3: white1x1.png をベースにUVからリアルな形状・質感を生成
    float2 uv = input.texcoord * 2.0f - 1.0f; // -1 ~ 1
    float shapeAlpha = 1.0f;
    float3 shade = float3(1.0f, 1.0f, 1.0f);

    if (input.mode < 1.5f)
    {
        // ===== モード1: 砂漠・砂嵐 =====
        float dist = length(uv);
        if (dist >= 1.0f)
        {
            discard;
        }

        if (input.seed < 0.5f)
        {
            // 砂煙・砂塵の塊: 柔らかいラジアル減衰 + FBMノイズで煙の濃淡を作る
            float radial = pow(saturate(1.0f - dist), 1.85f);
            float fbm = FbmDust(input.texcoord, input.seed * 2.0f);
            shapeAlpha = radial * lerp(0.35f, 1.25f, fbm);
            // 砂煙内部の立体感（上側がわずかに明るく、下側が濃い砂影）
            shade = lerp(float3(0.86f, 0.80f, 0.72f), float3(1.08f, 1.04f, 0.96f), saturate(fbm * 0.8f - uv.y * 0.25f));
        }
        else
        {
            // 高速砂粒: 楕円状に引き締まった粒状感
            float grain = pow(saturate(1.0f - dist), 1.3f);
            shapeAlpha = grain;
            shade = float3(1.1f, 1.06f, 0.98f);
        }
    }
    else if (input.mode < 2.5f)
    {
        // ===== モード2: 花びら =====
        float2 p = uv;
        p.x *= 1.25f;
        float d = length(p - float2(0.0f, 0.1f * abs(p.x)));
        if (d >= 0.95f)
        {
            discard;
        }
        shapeAlpha = smoothstep(0.95f, 0.65f, d);
        shade = lerp(float3(0.92f, 0.82f, 0.88f), float3(1.05f, 1.02f, 1.03f), saturate(0.5f - uv.y * 0.5f));
    }
    else
    {
        // ===== モード3: 嵐の夜 (3:雨 / 4:水しぶき) =====
        if (input.mode < 3.5f)
        {
            float streak = saturate(1.0f - abs(uv.x)) * saturate(1.0f - uv.y * uv.y);
            shapeAlpha = pow(streak, 1.2f);
        }
        else
        {
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            shapeAlpha = pow(saturate(1.0f - dist), 1.5f);
        }
    }

    if (shapeAlpha <= 0.004f)
    {
        discard;
    }

    float4 finalColor = gMaterial.color * textureColor * input.color;
    finalColor.rgb *= shade;
    finalColor.a *= saturate(shapeAlpha);
    output.color = finalColor;
    return output;
}