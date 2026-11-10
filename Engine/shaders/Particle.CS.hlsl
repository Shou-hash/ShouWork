#include "Particle.hlsli"

struct ParticleCSConstant
{
    float4x4 viewProjection;
    float4x4 billboard;
    float3 emitterPos;
    float deltaTime;
    float3 acceleration;
    uint maxParticles;
    float3 fieldMin;
    float pad0;
    float3 fieldMax;
    float pad1;
};

ConstantBuffer<ParticleCSConstant> gCSConstant : register(b0);
RWStructuredBuffer<ParticleForGPU> gParticles : register(u0);

// GPU用の簡易ハッシュ（疑似乱数）関数
float Hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return float(seed) / 4294967295.0f;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gCSConstant.maxParticles)
        return;

    ParticleForGPU p = gParticles[index];

    p.currentTime += gCSConstant.deltaTime;
    if (p.currentTime >= p.lifeTime)
    {
        // 寿命到達時の初期化処理 (Emit)
        float r1 = Hash(index + uint(p.currentTime * 1000.0f));
        float r2 = Hash(index * 13 + uint(p.currentTime * 500.0f));
        float r3 = Hash(index * 17 + uint(p.currentTime * 700.0f));

        p.currentTime = 0.0f;
        p.lifeTime = 1.0f + r1 * 2.0f;
        p.position = gCSConstant.emitterPos + float3((r1 - 0.5f) * 2.0f, (r2 - 0.5f) * 2.0f, (r3 - 0.5f) * 2.0f);
        p.velocity = float3((r2 - 0.5f) * 4.0f, (r3 - 0.5f) * 4.0f, (r1 - 0.5f) * 4.0f);
        p.scale = float3(1.0f, 1.0f, 1.0f);
        p.color = float4(r1, r2, r3, 1.0f);
    }
    else
    {
        // Acceleration Field 領域チェック
        if (p.position.x >= gCSConstant.fieldMin.x && p.position.x <= gCSConstant.fieldMax.x &&
            p.position.y >= gCSConstant.fieldMin.y && p.position.y <= gCSConstant.fieldMax.y &&
            p.position.z >= gCSConstant.fieldMin.z && p.position.z <= gCSConstant.fieldMax.z)
        {
            p.velocity += gCSConstant.acceleration * gCSConstant.deltaTime;
        }

        p.position += p.velocity * gCSConstant.deltaTime;
    }

    // 透明度フェードアウト
    p.color.a = 1.0f - (p.currentTime / p.lifeTime);

    // 行列計算（S * Billboard * T）
    float4x4 scaleMatrix =
    {
        p.scale.x, 0, 0, 0,
        0, p.scale.y, 0, 0,
        0, 0, p.scale.z, 0,
        0, 0, 0, 1
    };

    float4x4 translateMatrix =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        p.position.x, p.position.y, p.position.z, 1
    };

    float4x4 world = mul(mul(scaleMatrix, gCSConstant.billboard), translateMatrix);
    p.World = world;
    p.WVP = mul(world, gCSConstant.viewProjection);

    gParticles[index] = p;
}