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

    // モード1~4: white1x1.png をベースにUVからリアルな形状・質感を生成
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
            // 砂粒・堆積砂の塊: 微細なノイズで自然な砂の輪郭と陰影を作る
            float n = NoisePS(input.texcoord * 4.5f + input.seed * 23.0f);
            float edge = dist + (n - 0.5f) * 0.24f;
            if (edge >= 1.0f)
            {
                discard;
            }
            float grain = pow(saturate(1.0f - edge), 1.15f);
            shapeAlpha = grain;
            shade = lerp(float3(0.90f, 0.84f, 0.74f), float3(1.12f, 1.07f, 0.98f), saturate(n * 0.7f - uv.y * 0.3f));
        }
    }
    else if (input.mode < 2.5f)
    {
        // ===== モード2: 花びら・春の光の粒 =====
        if (input.seed < 0.5f)
        {
            // 花びら（先端に桜らしい切り込みを入れたハート楕円形状）
            float2 p = uv;
            p.x *= 1.28f;
            float notch = smoothstep(0.35f, 0.0f, abs(p.x)) * smoothstep(0.15f, 0.85f, -p.y) * 0.24f;
            float d = length(p - float2(0.0f, 0.12f * abs(p.x))) + notch;
            if (d >= 0.95f)
            {
                discard;
            }
            shapeAlpha = smoothstep(0.95f, 0.62f, d);
            // 花びらの付け根が濃く、先端に向かって淡くなるグラデーション
            shade = lerp(float3(0.90f, 0.76f, 0.84f), float3(1.06f, 1.03f, 1.05f), saturate(0.55f - uv.y * 0.55f));
        }
        else
        {
            // 春の陽光のボケ光
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            shapeAlpha = pow(saturate(1.0f - dist), 2.2f);
            shade = float3(1.15f, 1.12f, 1.08f);
        }
    }
    else if (input.mode < 7.5f)
    {
        // ===== モード3: 嵐の夜 (3:雨 / 4:水しぶき / 5:波紋 / 6:霧 / 7:稲妻) =====
        if (input.mode < 3.5f)
        {
            // 雨の筋
            float streak = saturate(1.0f - abs(uv.x)) * saturate(1.0f - uv.y * uv.y);
            shapeAlpha = pow(streak, 1.2f);
        }
        else if (input.mode < 4.5f)
        {
            // 水しぶき
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            shapeAlpha = pow(saturate(1.0f - dist), 1.5f);
            shade = float3(1.08f, 1.12f, 1.20f);
        }
        else if (input.mode < 5.5f)
        {
            // 水たまりの波紋リング
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            float ring = 1.0f - abs(dist - 0.76f) / 0.24f;
            shapeAlpha = pow(saturate(ring), 1.8f);
            shade = float3(1.05f, 1.12f, 1.25f);
        }
        else if (input.mode < 6.5f)
        {
            // 低空を流れる嵐の霧
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            float radial = pow(saturate(1.0f - dist), 1.9f);
            float fbm = FbmDust(input.texcoord, input.seed * 2.0f);
            shapeAlpha = radial * lerp(0.4f, 1.2f, fbm);
        }
        else
        {
            // 稲妻・電撃の閃光
            float core = pow(saturate(1.0f - abs(uv.x)), 2.4f) * saturate(1.0f - uv.y * uv.y * 0.6f);
            if (core <= 0.01f)
            {
                discard;
            }
            shapeAlpha = core;
            shade = lerp(float3(0.75f, 0.92f, 1.35f), float3(1.6f, 1.65f, 1.8f), core);
        }
    }
    else
    {
        // ===== モード4: 雪景色 (8:雪の結晶 / 9:綿雪・積雪 / 10:ダイヤモンドダスト / 11:オーロラ) =====
        if (input.mode < 8.5f)
        {
            // 六角形の雪の結晶（スノーフレーク）
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            float angle = atan2(uv.y, uv.x);
            float sector = abs(frac(angle / 1.0471975f + 0.5f) - 0.5f) * 1.0471975f;
            float2 p = float2(cos(sector), sin(sector)) * dist;

            // 主軸と樹枝状のサブブランチ
            float mainArm = smoothstep(0.11f, 0.02f, p.y) * smoothstep(0.95f, 0.65f, p.x);
            float sub1 = smoothstep(0.08f, 0.015f, abs((p.x - 0.35f) * 0.65f - p.y)) * step(0.35f, p.x) * step(p.x, 0.72f);
            float sub2 = smoothstep(0.07f, 0.015f, abs((p.x - 0.58f) * 0.65f - p.y)) * step(0.58f, p.x) * step(p.x, 0.88f);
            float core = smoothstep(0.28f, 0.05f, dist);

            shapeAlpha = saturate(max(max(mainArm, max(sub1, sub2)), core));
            shade = lerp(float3(0.84f, 0.94f, 1.08f), float3(1.18f, 1.22f, 1.28f), core);
        }
        else if (input.mode < 9.5f)
        {
            // ふわふわの綿雪・積雪の塊
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            float n = NoisePS(input.texcoord * 3.8f + input.seed * 17.0f);
            float edge = dist + (n - 0.5f) * 0.16f;
            if (edge >= 1.0f)
            {
                discard;
            }
            shapeAlpha = pow(saturate(1.0f - edge), 1.45f);
            shade = lerp(float3(0.86f, 0.93f, 1.04f), float3(1.08f, 1.10f, 1.14f), saturate(0.6f - uv.y * 0.4f));
        }
        else if (input.mode < 10.5f)
        {
            // ダイヤモンドダスト（十字に輝く光の粒子）
            float dist = length(uv);
            if (dist >= 1.0f)
            {
                discard;
            }
            float glow = pow(saturate(1.0f - dist), 2.6f);
            float crossX = pow(saturate(1.0f - abs(uv.y) * 5.5f), 2.0f) * saturate(1.0f - abs(uv.x));
            float crossY = pow(saturate(1.0f - abs(uv.x) * 5.5f), 2.0f) * saturate(1.0f - abs(uv.y));
            shapeAlpha = saturate(glow + (crossX + crossY) * 0.85f);
            shade = float3(1.25f, 1.32f, 1.45f);
        }
        else
        {
            // オーロラカーテン（縦に伸びる極光の光の帯）
            float horizFade = pow(saturate(1.0f - abs(uv.x)), 1.65f);
            float vertFade = smoothstep(1.0f, 0.15f, abs(uv.y)) * smoothstep(-1.0f, 0.45f, uv.y);
            float ray = NoisePS(float2(input.texcoord.x * 9.0f + input.seed * 31.0f, input.texcoord.y * 1.4f));
            float fbm = FbmDust(float2(input.texcoord.x * 2.0f, input.texcoord.y * 0.6f), input.seed);
            shapeAlpha = horizFade * vertFade * lerp(0.45f, 1.35f, ray * 0.6f + fbm * 0.4f);
            shade = lerp(float3(0.85f, 1.05f, 1.0f), float3(1.25f, 1.35f, 1.30f), saturate(0.5f - uv.y * 0.5f));
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