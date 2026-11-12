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

    // ---- ここから追加(C++側 ParticleCSConstant と同じ並び) ----
    uint mode;
    uint frame;
    uint activeCount;
    uint reset;
    float screenW;
    float screenH;
    float mouseX;
    float mouseY;
    float mouseRadius;
    float mouseForce;
    float mouseDown;
    float time;
    float4 colorA;
    float4 colorB;
    float sizeMin;
    float sizeMax;
    float lifeMin;
    float lifeMax;
    float speedMin;
    float speedMax;
    float wind;
    float turbulence;
    float stretch;
    float3 pad2;
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

// 粒ごと・フレームごとに変わる乱数 (0~1)
float Rand(uint index, uint salt)
{
    return Hash(index * 7919u + salt * 104729u + gCSConstant.frame * 2654435761u);
}

// マウスによる押しのけ。swirl>0 で渦を巻く(花びら)。hard=true でマウス内側に入れない(砂)
void ApplyMouse(inout ParticleForGPU p, float swirl, bool hard)
{
    float2 mouse = float2(gCSConstant.mouseX, gCSConstant.mouseY);
    float2 d = p.position.xy - mouse;
    float dist = length(d);
    float r = gCSConstant.mouseRadius;
    if (dist >= r)
        return;

    float2 dir = dist > 0.001f ? d / dist : float2(1.0f, 0.0f);
    float k = 1.0f - dist / r;
    float push = k * k * gCSConstant.mouseForce * (1.0f + gCSConstant.mouseDown * 2.0f);
    p.velocity.xy += (dir + float2(-dir.y, dir.x) * swirl) * push * gCSConstant.deltaTime;

    if (hard && dist < r * 0.6f)
        p.position.xy = mouse + dir * (r * 0.6f);
}

// 粒の(再)生成。座標は画面ピクセル(左上原点)。
// 使い回しフィールド: scale.z=基準アルファ, position.z=粒ごとの値(花びら:落下速度 / 雨:着地Y), pad=雨の状態(0:落下 1:水しぶき)
void Emit(inout ParticleForGPU p, uint index, bool fill)
{
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));
    float speed = lerp(gCSConstant.speedMin, gCSConstant.speedMax, Rand(index, 2));
    float4 col = lerp(gCSConstant.colorA, gCSConstant.colorB, Rand(index, 3));
    float rx = Rand(index, 4);
    float ry = Rand(index, 5);

    p.currentTime = 0.0f;
    p.lifeTime = lerp(gCSConstant.lifeMin, gCSConstant.lifeMax, Rand(index, 6));
    p.pad = 0.0f;
    p.color = float4(col.rgb, 1.0f);
    p.scale = float3(size, size * gCSConstant.stretch, col.a);
    p.position.z = 0.0f;

    if (gCSConstant.mode == 1) // 砂漠: 左から右へ流れる
    {
        p.position.xy = float2(fill ? rx * W : -size, ry * H);
        p.velocity = float3(speed, (Rand(index, 7) - 0.5f) * speed * 0.2f, 0.0f);
    }
    else if (gCSConstant.mode == 2) // 花びら: 上から舞い落ちる
    {
        p.position.xy = float2(rx * (W + 300.0f) - 300.0f, fill ? ry * H : -size * 2.0f);
        p.velocity = float3(gCSConstant.wind, speed, 0.0f);
        p.position.z = speed;
    }
    else // 嵐: 上から高速で降る雨
    {
        p.position.xy = float2(rx * (W + 400.0f) - 200.0f, fill ? ry * H : -p.scale.y);
        p.velocity = float3(gCSConstant.wind, speed, 0.0f);
        p.position.z = H - Rand(index, 7) * 140.0f - 10.0f; // 着地ライン(奥行き感のため幅を持たせる)
    }
}

// 1フレーム分の挙動。画面外に出て作り直しが必要なら true
bool Step(inout ParticleForGPU p, uint index)
{
    float dt = gCSConstant.deltaTime;
    float t = gCSConstant.time;
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float phase = float(index % 997u);

    if (gCSConstant.mode == 1)
    {
        p.velocity.xy += (float2(gCSConstant.wind, 0.0f) - p.velocity.xy) * 0.8f * dt;
        p.velocity.y += sin(t * 1.7f + phase) * gCSConstant.turbulence * dt;
        ApplyMouse(p, 0.0f, true);
        p.position.xy += p.velocity.xy * dt;
        return p.position.x > W + 300.0f || p.position.y < -300.0f || p.position.y > H + 300.0f;
    }

    if (gCSConstant.mode == 2)
    {
        float sway = sin(t * 1.5f + phase) * gCSConstant.turbulence;
        p.velocity.x += (gCSConstant.wind + sway - p.velocity.x) * 2.0f * dt;
        p.velocity.y += (p.position.z - p.velocity.y) * 1.5f * dt;
        ApplyMouse(p, 0.6f, false);
        p.position.xy += p.velocity.xy * dt;
        return p.position.y > H + 50.0f || p.position.x > W + 300.0f;
    }

    // mode 3: 嵐の夜
    if (p.pad < 0.5f)
    {
        p.velocity.x = gCSConstant.wind + sin(t * 0.8f + phase) * gCSConstant.turbulence;
        p.position.xy += p.velocity.xy * dt;

        // マウスを中心とした円盤(半径=mouseRadius/2)に当たったら水しぶき
        float2 dm = p.position.xy - float2(gCSConstant.mouseX, gCSConstant.mouseY);
        float r = gCSConstant.mouseRadius * 0.5f;
        bool hitMouse = dot(dm, dm) < r * r;
        bool hitGround = p.position.y >= p.position.z;
        if (hitMouse || hitGround)
        {
            p.pad = 1.0f;
            p.currentTime = 0.0f;
            p.lifeTime = 0.3f + Rand(index, 8) * 0.3f;
            p.velocity = float3((Rand(index, 9) - 0.5f) * 260.0f, -(80.0f + Rand(index, 10) * 260.0f), 0.0f);
            float s = hitMouse ? 12.0f : 7.0f;
            p.scale = float3(s, s, 0.9f);
            p.color = float4(0.8f, 0.9f, 1.0f, 1.0f);
        }
    }
    else
    {
        p.velocity.y += 900.0f * dt;
        p.position.xy += p.velocity.xy * dt;
    }
    return false;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gCSConstant.maxParticles)
        return;

    ParticleForGPU p = gParticles[index];
    bool reset = gCSConstant.reset != 0;

    if (gCSConstant.mode == 0)
    {
        // ===== 既存の演出(従来どおり) =====
        p.currentTime += gCSConstant.deltaTime;
        if (reset || p.currentTime >= p.lifeTime)
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
    }
    else if (index >= gCSConstant.activeCount)
    {
        // Editorで count を減らした分は非表示(次に増えたとき lifeTime=0 で作り直される)
        p.currentTime = 0.0f;
        p.lifeTime = 0.0f;
        p.scale = float3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        // ===== モード1~3: 画面ピクセル座標で動く演出 =====
        bool fill = reset || p.lifeTime == 0.0f; // 画面全体にばらまいて作る
        p.currentTime += gCSConstant.deltaTime;

        bool respawn = fill || p.currentTime >= p.lifeTime;
        if (!respawn)
            respawn = Step(p, index);
        if (respawn)
            Emit(p, index, fill);

        float t = p.currentTime;
        float fade;
        if (gCSConstant.mode == 3)
            fade = (p.pad < 0.5f) ? 1.0f : 1.0f - t / p.lifeTime; // 雨は常に不透明、しぶきは消える
        else
            fade = saturate(min(t, p.lifeTime - t) / 0.5f); // 出現・消滅で0.5秒フェード
        p.color.a = p.scale.z * fade;
    }

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
