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

// 粒固有の固定乱数 (0~1)
float FixedRand(uint index, uint salt)
{
    return Hash(index * 16127u + salt * 31337u + 1337u);
}

// 2D空間バリューノイズ（気流のうねり生成用）
float Hash2D(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Noise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float a = Hash2D(i);
    float b = Hash2D(i + float2(1.0f, 0.0f));
    float c = Hash2D(i + float2(0.0f, 1.0f));
    float d = Hash2D(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// 空気の渦・気流ベクトル（空間的に連続したうねりを作る）
float2 SampleAirFlow(float2 pos, float t)
{
    float2 uv = float2(pos.x * 0.0028f - t * 0.55f, pos.y * 0.0042f);
    float n1 = Noise2D(uv) * 2.0f - 1.0f;
    float n2 = Noise2D(uv * 2.15f + float2(13.7f, -7.3f)) * 2.0f - 1.0f;
    return float2(n1 * 0.6f + n2 * 0.4f, n1 * 0.8f - n2 * 0.5f);
}

// 砂漠パーティクルの層判定 (0:巨大砂煙, 1:中層砂塵の帯, 2:高速砂粒)
uint GetDesertLayer(uint index)
{
    float r = float(index) / max(float(gCSConstant.activeCount), 1.0f);
    if (r < 0.22f)
        return 0u;
    if (r < 0.58f)
        return 1u;
    return 2u;
}

// マウスによる気流干渉
void ApplyMouse(inout ParticleForGPU p, float swirl, bool aerodynamic)
{
    float2 mouse = float2(gCSConstant.mouseX, gCSConstant.mouseY);
    float2 d = p.position.xy - mouse;
    float dist = length(d);
    float r = gCSConstant.mouseRadius;
    if (dist >= r)
        return;

    float2 dir = dist > 0.001f ? d / dist : float2(0.0f, -1.0f);
    float k = 1.0f - dist / r;
    float power = k * k * gCSConstant.mouseForce * (1.0f + gCSConstant.mouseDown * 2.5f);

    if (aerodynamic)
    {
        // 障害物を回り込む風＋後流の巻き上げ渦（不自然な丸い穴を作らない）
        float2 tangent = float2(-dir.y, dir.x);
        float side = (d.y >= 0.0f) ? 1.0f : -1.0f;
        float wake = saturate((d.x + r * 0.3f) / r);
        float2 flow = dir * 0.45f + tangent * side * 0.85f + float2(0.2f, -0.55f) * wake;
        p.velocity.xy += flow * power * gCSConstant.deltaTime;
    }
    else
    {
        p.velocity.xy += (dir + float2(-dir.y, dir.x) * swirl) * power * gCSConstant.deltaTime;
    }
}

// 粒の(再)生成。座標は画面ピクセル(左上原点)。
// scale.z = 基準アルファ, position.z = 粒ごとの固有値(砂漠:回転角 / 花びら:落下速度 / 雨:着地Y)
void Emit(inout ParticleForGPU p, uint index, bool fill)
{
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float speed = lerp(gCSConstant.speedMin, gCSConstant.speedMax, Rand(index, 2));
    float4 col = lerp(gCSConstant.colorA, gCSConstant.colorB, Rand(index, 3));
    float rx = Rand(index, 4);
    float ry = Rand(index, 5);
    float grainSeed = FixedRand(index, 9);

    p.currentTime = 0.0f;
    p.lifeTime = lerp(gCSConstant.lifeMin, gCSConstant.lifeMax, Rand(index, 6));
    p.color = float4(col.rgb, 1.0f);
    p.position.z = 0.0f;

    if (gCSConstant.mode == 1) // 砂漠・砂嵐: 3層構造でリアルな砂塵を表現
    {
        uint layer = GetDesertLayer(index);
        float baseSize = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));

        if (layer == 0u)
        {
            // 層0: 遠景〜中景を覆う巨大な砂煙（柔らかく薄い）
            float s = lerp(gCSConstant.sizeMax * 1.4f, gCSConstant.sizeMax * 2.8f, Rand(index, 1));
            p.position.xy = float2(fill ? rx * (W + s) - s * 0.5f : -s * 0.6f, ry * H);
            p.velocity = float3(speed * lerp(0.35f, 0.55f, Rand(index, 7)), (Rand(index, 8) - 0.5f) * 35.0f, 0.0f);
            p.scale = float3(s * 1.25f, s * 0.9f * gCSConstant.stretch, col.a * 0.30f);
            p.position.z = Rand(index, 10) * 6.283185f; // 初期回転角
            p.pad = 1.0f + grainSeed * 0.45f; // 1.0~1.45: 柔らかい砂煙シェーダー
        }
        else if (layer == 1u)
        {
            // 層1: 地表付近〜中層をうねる砂塵の帯（下部に濃く分布）
            float yBias = pow(ry, 0.72f);
            float s = lerp(gCSConstant.sizeMin * 2.2f, gCSConstant.sizeMax * 1.15f, Rand(index, 1));
            p.position.xy = float2(fill ? rx * (W + s) - s * 0.5f : -s * 0.6f, yBias * H);
            p.velocity = float3(speed * lerp(0.75f, 1.15f, Rand(index, 7)), (Rand(index, 8) - 0.55f) * 60.0f, 0.0f);
            p.scale = float3(s * 1.6f, s * 0.65f * gCSConstant.stretch, col.a * 0.52f);
            p.position.z = (Rand(index, 10) - 0.5f) * 0.5f;
            p.pad = 1.0f + grainSeed * 0.45f; // 1.0~1.45: 柔らかい砂煙シェーダー
        }
        else
        {
            // 層2: 手前を猛スピードで吹き飛ぶ細かい砂粒（サルテーション・高速飛翔）
            float yBias = pow(ry, 0.65f);
            float grainSize = lerp(2.2f, 6.8f, Rand(index, 1) * Rand(index, 1));
            float fastSpeed = speed * lerp(1.35f, 2.35f, Rand(index, 7));
            float streak = clamp(fastSpeed / 260.0f, 1.5f, 6.5f) * gCSConstant.stretch;
            p.position.xy = float2(fill ? rx * W : -grainSize * streak, yBias * H);
            p.velocity = float3(fastSpeed, (Rand(index, 8) - 0.65f) * fastSpeed * 0.22f, 0.0f);
            p.scale = float3(grainSize * streak, grainSize, min(col.a * 1.35f, 0.95f));
            p.position.z = 0.0f;
            // 明るめの砂色に寄せる
            p.color.rgb = lerp(p.color.rgb, float3(1.0f, 0.92f, 0.76f), 0.35f);
            p.pad = 1.52f + grainSeed * 0.45f; // 1.52~1.97: シャープな砂粒シェーダー
        }
    }
    else if (gCSConstant.mode == 2) // 花びら: 上から舞い落ちる
    {
        float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));
        p.position.xy = float2(rx * (W + 300.0f) - 300.0f, fill ? ry * H : -size * 2.0f);
        p.velocity = float3(gCSConstant.wind, speed, 0.0f);
        p.scale = float3(size, size * gCSConstant.stretch, col.a);
        p.position.z = speed;
        p.pad = 2.0f + grainSeed * 0.9f;
    }
    else // 嵐: 上から高速で降る雨
    {
        float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));
        p.scale = float3(size, size * gCSConstant.stretch, col.a);
        p.position.xy = float2(rx * (W + 400.0f) - 200.0f, fill ? ry * H : -p.scale.y);
        p.velocity = float3(gCSConstant.wind, speed, 0.0f);
        p.position.z = H - Rand(index, 7) * 140.0f - 10.0f; // 着地ライン(奥行き感のため幅を持たせる)
        p.pad = 3.0f;
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
        uint layer = GetDesertLayer(index);

        // 突風（Gust）の周期的なうねり
        float gust = 1.0f + 0.42f * sin(t * 0.85f) + 0.22f * sin(t * 2.13f + p.position.x * 0.002f);
        float2 airFlow = SampleAirFlow(p.position.xy, t);

        if (layer == 0u)
        {
            // 巨大な砂煙: ゆったりとした気流に乗って回転しながら流れる
            float targetVx = gCSConstant.wind * 0.45f * gust + airFlow.x * gCSConstant.turbulence * 0.7f;
            float targetVy = airFlow.y * gCSConstant.turbulence * 0.55f;
            p.velocity.xy += (float2(targetVx, targetVy) - p.velocity.xy) * 1.1f * dt;
            p.position.z += ((index % 2u == 0u) ? 0.18f : -0.18f) * dt; // ゆっくり渦回転
            ApplyMouse(p, 0.0f, true);
            p.position.xy += p.velocity.xy * dt;
            float margin = p.scale.x * 0.75f + 150.0f;
            return p.position.x > W + margin || p.position.y < -margin || p.position.y > H + margin;
        }
        else if (layer == 1u)
        {
            // 中層の砂塵の帯: 突風と空間乱流で波打ちながら進む
            float targetVx = gCSConstant.wind * 0.95f * gust + airFlow.x * gCSConstant.turbulence * 1.2f;
            float targetVy = airFlow.y * gCSConstant.turbulence * 1.1f - 12.0f;
            p.velocity.xy += (float2(targetVx, targetVy) - p.velocity.xy) * 1.8f * dt;
            p.position.z = atan2(p.velocity.y, max(p.velocity.x, 1.0f)) * 0.6f; // 風向きに沿って傾く
            ApplyMouse(p, 0.0f, true);
            p.position.xy += p.velocity.xy * dt;
            float margin = p.scale.x * 0.75f + 120.0f;
            return p.position.x > W + margin || p.position.y < -margin || p.position.y > H + margin;
        }
        else
        {
            // 高速砂粒: 強い風と重力＋地表バウンド（サルテーション）
            float targetVx = gCSConstant.wind * 1.85f * gust + airFlow.x * gCSConstant.turbulence * 1.8f;
            p.velocity.x += (targetVx - p.velocity.x) * 2.5f * dt;
            p.velocity.y += (180.0f + airFlow.y * gCSConstant.turbulence * 2.4f) * dt; // 重力＋乱流
            ApplyMouse(p, 0.0f, true);
            p.position.xy += p.velocity.xy * dt;

            // 画面下端（砂丘表面）に当たったら跳ね返って舞い上がる
            float groundY = H - 4.0f - FixedRand(index, 11) * 28.0f;
            if (p.position.y >= groundY && p.velocity.y > 0.0f)
            {
                p.position.y = groundY;
                p.velocity.y = -abs(p.velocity.y) * lerp(0.35f, 0.75f, Rand(index, 12)) - 40.0f;
                p.velocity.x *= 0.88f;
            }
            return p.position.x > W + 120.0f || p.position.y < -150.0f;
        }
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

    // mode 3: 嵐の夜 (pad 3.0=落下中, 4.0=水しぶき)
    if (p.pad < 3.5f)
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
            p.pad = 4.0f;
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
    float rotAngle = 0.0f;

    if (gCSConstant.mode == 0)
    {
        // ===== 既存の演出(従来どおり) =====
        p.currentTime += gCSConstant.deltaTime;
        p.pad = 0.0f;
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
        {
            fade = (p.pad < 3.5f) ? 1.0f : 1.0f - t / p.lifeTime; // 雨は常に不透明、しぶきは消える
            if (p.pad < 3.5f)
                rotAngle = atan2(p.velocity.x, max(p.velocity.y, 1.0f)) * -0.5f;
        }
        else if (gCSConstant.mode == 2)
        {
            fade = saturate(min(t, p.lifeTime - t) / 0.5f);
            rotAngle = gCSConstant.time * 2.2f + float(index % 628u) * 0.01f;
        }
        else
        {
            // 砂漠モード: 出現・消滅を滑らかにフェード
            fade = saturate(min(t, p.lifeTime - t) / 0.8f);
            uint layer = GetDesertLayer(index);
            if (layer == 2u)
            {
                // 高速砂粒は進行方向に傾けてスピード感を出す
                rotAngle = atan2(p.velocity.y, max(abs(p.velocity.x), 1.0f));
            }
            else
            {
                rotAngle = p.position.z;
            }
        }
        p.color.a = p.scale.z * fade;
    }

    // 行列計算（Scale * RotateZ * Billboard * Translate）
    float4x4 scaleMatrix =
    {
        p.scale.x, 0, 0, 0,
        0, p.scale.y, 0, 0,
        0, 0, 1.0f, 0,
        0, 0, 0, 1
    };

    float c = cos(rotAngle);
    float s = sin(rotAngle);
    float4x4 rotMatrix =
    {
        c, s, 0, 0,
       -s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    float4x4 translateMatrix =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        p.position.x, p.position.y, p.position.z, 1
    };

    float4x4 world = mul(mul(mul(scaleMatrix, rotMatrix), gCSConstant.billboard), translateMatrix);
    p.World = world;
    p.WVP = mul(world, gCSConstant.viewProjection);

    gParticles[index] = p;
}