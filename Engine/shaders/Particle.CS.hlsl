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
    float tornado;
    float mouseVelX;
    float mouseVelY;
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

// 砂丘の地表高さ（画面下部に緩やかな起伏を作る）
float GetDuneSurfaceY(float x, float W, float H)
{
    float nx = x / max(W, 1.0f);
    float dune1 = sin(nx * 6.283185f * 1.35f + 0.6f) * 18.0f;
    float dune2 = sin(nx * 6.283185f * 3.1f - 1.1f) * 7.0f;
    return H - 62.0f - dune1 - dune2;
}

// 粒ごとの目標堆積Y座標
float GetParticleDepositY(uint index, float x, float W, float H)
{
    float surfaceY = GetDuneSurfaceY(x, W, H);
    float depth = FixedRand(index, 13) * 58.0f;
    return min(surfaceY + depth, H - 3.0f);
}

// 砂漠パーティクルの層判定 (0:巨大砂煙, 1:中層砂塵の帯, 2:飛翔砂粒, 3:地面の堆積砂)
uint GetDesertLayer(uint index)
{
    float r = float(index) / max(float(gCSConstant.activeCount), 1.0f);
    if (r < 0.16f)
        return 0u;
    if (r < 0.40f)
        return 1u;
    if (r < 0.74f)
        return 2u;
    return 3u;
}

// 花びらパーティクルの層判定 (0:遠景のぼかし・光の粒, 1:空中を舞う花びら, 2:地面に積もる花びら)
uint GetPetalLayer(uint index)
{
    float r = float(index) / max(float(gCSConstant.activeCount), 1.0f);
    if (r < 0.16f)
        return 0u;
    if (r < 0.72f)
        return 1u;
    return 2u;
}

// 花びらの地表高さ（画面下部に緩やかな花びらの絨毯を作る）
float GetPetalGroundY(uint index, float x, float W, float H)
{
    float nx = x / max(W, 1.0f);
    float wave = sin(nx * 6.283185f * 1.8f + 0.4f) * 8.0f + cos(nx * 6.283185f * 4.2f) * 4.0f;
    float surfaceY = H - 34.0f - wave;
    float depth = FixedRand(index, 31) * 32.0f;
    return min(surfaceY + depth, H - 3.0f);
}

// 嵐パーティクルの層判定 (0:低空の霧・雨雲, 1:奥の細かい霧雨, 2:手前の豪雨・波紋)
uint GetStormLayer(uint index)
{
    float r = float(index) / max(float(gCSConstant.activeCount), 1.0f);
    if (r < 0.10f)
        return 0u;
    if (r < 0.45f)
        return 1u;
    return 2u;
}

// 雪景色パーティクルの層判定 (0:遠景のボケ雪・冷気, 1:空中を舞う雪の結晶・綿雪, 2:地面に積もる雪原)
uint GetSnowLayer(uint index)
{
    float r = float(index) / max(float(gCSConstant.activeCount), 1.0f);
    if (r < 0.15f)
        return 0u;
    if (r < 0.74f)
        return 1u;
    return 2u;
}

// 雪原の地表高さ（画面下部に緩やかな雪の起伏を作る）
float GetSnowGroundY(uint index, float x, float W, float H)
{
    float nx = x / max(W, 1.0f);
    float drift1 = sin(nx * 6.283185f * 1.2f + 0.3f) * 14.0f;
    float drift2 = cos(nx * 6.283185f * 2.8f - 0.7f) * 6.0f;
    float surfaceY = H - 46.0f - drift1 - drift2;
    float depth = FixedRand(index, 51) * 44.0f;
    return min(surfaceY + depth, H - 3.0f);
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
        // 障害物を回り込む風＋後流の巻き上げ渦＋マウス移動の引きずり
        float2 tangent = float2(-dir.y, dir.x);
        float side = (d.y >= 0.0f) ? 1.0f : -1.0f;
        float wake = saturate((d.x + r * 0.3f) / r);
        float2 mouseVel = float2(gCSConstant.mouseVelX, gCSConstant.mouseVelY);
        float2 flow = dir * 0.45f + tangent * side * 0.85f + float2(0.2f, -0.55f) * wake;
        p.velocity.xy += (flow * power + mouseVel * k * 2.2f) * gCSConstant.deltaTime;
    }
    else
    {
        p.velocity.xy += (dir + float2(-dir.y, dir.x) * swirl) * power * gCSConstant.deltaTime;
    }
}

// 貯まっている砂場に対するマウスの反応（移動すると砂が舞い散る）
void ApplyMouseToSettledSand(inout ParticleForGPU p, uint index)
{
    float2 mouse = float2(gCSConstant.mouseX, gCSConstant.mouseY);
    float2 d = p.position.xy - mouse;
    float dist = length(d);
    float r = gCSConstant.mouseRadius * 0.85f;
    if (dist >= r)
        return;

    float2 mouseVel = float2(gCSConstant.mouseVelX, gCSConstant.mouseVelY);
    float mouseSpeed = length(mouseVel);
    float k = 1.0f - dist / r;

    // マウスが砂場で動いたとき、またはクリック中に砂を巻き上げる
    float triggerSpeed = (gCSConstant.mouseDown > 0.5f) ? 5.0f : 35.0f;
    if (mouseSpeed > triggerSpeed || gCSConstant.mouseDown > 0.5f)
    {
        float2 dir = dist > 0.001f ? d / dist : float2(0.0f, -1.0f);
        float boost = 1.0f + gCSConstant.mouseDown * 1.6f;
        float kick = clamp(mouseSpeed * 0.45f, 120.0f, 680.0f) * k * boost;

        // 堆積状態を解除して空中へ舞い上げる
        p.velocity.z = 0.0f;
        p.velocity.x = mouseVel.x * 0.55f * k + dir.x * kick + (Rand(index, 14) - 0.35f) * 240.0f;
        p.velocity.y = -abs(kick) * lerp(0.55f, 1.15f, Rand(index, 15)) - 90.0f + mouseVel.y * 0.25f * k;
        p.position.y -= 3.0f;

        // 舞い上がる砂粒の見た目に切り替え
        float grainSize = lerp(3.0f, 7.5f, FixedRand(index, 16));
        p.scale = float3(grainSize * 2.2f, grainSize, 0.88f);
        p.pad = 1.6f + FixedRand(index, 9) * 0.35f;
        p.currentTime = min(p.currentTime, p.lifeTime * 0.4f);
    }
}

// 地面に積もった花びらに対するマウスの反応（移動すると花びらが舞い上がる）
void ApplyMouseToSettledPetals(inout ParticleForGPU p, uint index)
{
    float2 mouse = float2(gCSConstant.mouseX, gCSConstant.mouseY);
    float2 d = p.position.xy - mouse;
    float dist = length(d);
    float r = gCSConstant.mouseRadius * 0.9f;
    if (dist >= r)
        return;

    float2 mouseVel = float2(gCSConstant.mouseVelX, gCSConstant.mouseVelY);
    float mouseSpeed = length(mouseVel);
    float k = 1.0f - dist / r;

    float triggerSpeed = (gCSConstant.mouseDown > 0.5f) ? 5.0f : 30.0f;
    if (mouseSpeed > triggerSpeed || gCSConstant.mouseDown > 0.5f)
    {
        float2 dir = dist > 0.001f ? d / dist : float2(0.0f, -1.0f);
        float boost = 1.0f + gCSConstant.mouseDown * 1.4f;
        float kick = clamp(mouseSpeed * 0.35f, 80.0f, 420.0f) * k * boost;

        // 堆積状態を解除して空中へ舞い上げる
        p.velocity.z = 0.0f;
        p.velocity.x = mouseVel.x * 0.45f * k + dir.x * kick + (Rand(index, 32) - 0.5f) * 140.0f;
        p.velocity.y = -abs(kick) * lerp(0.5f, 1.05f, Rand(index, 33)) - 60.0f;
        p.position.y -= 4.0f;
        p.position.z = lerp(gCSConstant.speedMin, gCSConstant.speedMax, FixedRand(index, 34)) * 0.7f;
        p.currentTime = min(p.currentTime, p.lifeTime * 0.45f);
    }
}

// 地面に積もった雪に対するマウスの反応（移動するとパウダースノーが舞い上がる）
void ApplyMouseToSettledSnow(inout ParticleForGPU p, uint index)
{
    float2 mouse = float2(gCSConstant.mouseX, gCSConstant.mouseY);
    float2 d = p.position.xy - mouse;
    float dist = length(d);
    float r = gCSConstant.mouseRadius * 0.9f;
    if (dist >= r)
        return;

    float2 mouseVel = float2(gCSConstant.mouseVelX, gCSConstant.mouseVelY);
    float mouseSpeed = length(mouseVel);
    float k = 1.0f - dist / r;

    float triggerSpeed = (gCSConstant.mouseDown > 0.5f) ? 5.0f : 28.0f;
    if (mouseSpeed > triggerSpeed || gCSConstant.mouseDown > 0.5f)
    {
        float2 dir = dist > 0.001f ? d / dist : float2(0.0f, -1.0f);
        float boost = 1.0f + gCSConstant.mouseDown * 1.5f;
        float kick = clamp(mouseSpeed * 0.35f, 70.0f, 380.0f) * k * boost;

        // 積雪状態を解除してパウダースノーとして舞い上げる
        p.velocity.z = 0.0f;
        p.velocity.x = mouseVel.x * 0.42f * k + dir.x * kick + (Rand(index, 52) - 0.5f) * 110.0f;
        p.velocity.y = -abs(kick) * lerp(0.45f, 0.95f, Rand(index, 53)) - 55.0f;
        p.position.y -= 4.0f;
        p.position.z = lerp(gCSConstant.speedMin, gCSConstant.speedMax, FixedRand(index, 54)) * 0.65f;
        float powderS = lerp(4.0f, 9.0f, FixedRand(index, 55));
        p.scale = float3(powderS, powderS, 0.85f);
        p.pad = 9.0f + FixedRand(index, 9) * 0.45f;
        p.currentTime = min(p.currentTime, p.lifeTime * 0.45f);
    }
}

// スペースキー押下中の竜巻による巻き上げ処理（モード1:砂漠）
void ApplyTornado(inout ParticleForGPU p, uint index, uint layer)
{
    float dt = gCSConstant.deltaTime;
    float t = gCSConstant.time;
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float phase = float(index % 997u);

    // 竜巻の中心X位置（画面中央付近を蛇行しつつマウス位置にもわずかに寄る）
    float baseX = (gCSConstant.mouseX > -5000.0f) ? lerp(W * 0.5f, clamp(gCSConstant.mouseX, W * 0.15f, W * 0.85f), 0.35f) : W * 0.5f;
    float centerX = baseX + sin(t * 1.25f) * 150.0f + sin(t * 2.65f) * 48.0f;

    // 高さに応じた漏斗（じょうご）形状（下は細く、上空ほど大きく広がる）
    float hNorm = saturate((H - p.position.y) / max(H, 1.0f));
    float funnelRadius = lerp(38.0f, 340.0f, pow(hNorm, 1.15f));
    float spineX = centerX + sin(hNorm * 5.2f - t * 4.5f) * (32.0f * hNorm + 8.0f);

    float dx = p.position.x - spineX;
    float absDx = abs(dx);
    float pullRange = funnelRadius + lerp(280.0f, 460.0f, 1.0f - hNorm);

    // 地面に貯まっている砂も竜巻の吸引圏内なら引き剥がして巻き上げる
    if (p.velocity.z > 0.5f)
    {
        if (absDx < pullRange)
        {
            p.velocity.z = 0.0f;
            p.position.y -= 4.0f + Rand(index, 17) * 8.0f;
            p.velocity.y = -lerp(180.0f, 420.0f, Rand(index, 18));
            float grainSize = lerp(3.2f, 8.5f, FixedRand(index, 16));
            p.scale = float3(grainSize * 2.4f, grainSize, 0.90f);
            p.pad = 1.6f + FixedRand(index, 9) * 0.35f;
        }
        return;
    }

    // 立体的な螺旋回転（角速度＋漏斗の縁に沿った軌道）
    float swirlSpeed = lerp(11.5f, 6.2f, hNorm) + FixedRand(index, 19) * 3.5f;
    float swirlAngle = t * swirlSpeed + phase * 0.41f + hNorm * 6.5f;
    float radialRatio = lerp(0.12f, 1.05f, FixedRand(index, 20));
    float orbitRadius = funnelRadius * radialRatio;
    float targetX = spineX + cos(swirlAngle) * orbitRadius;
    float depthZ = sin(swirlAngle); // -1(奥) ~ +1(手前)

    // 中心軸への強い吸引＋螺旋軌道への追従＋上空の横風による吹き流し
    float attractStrength = (absDx < pullRange) ? lerp(11.0f, 4.5f, hNorm) : 2.2f;
    float windDrift = gCSConstant.wind * (0.18f + 0.85f * pow(hNorm, 1.4f));
    float desiredVx = (targetX - p.position.x) * attractStrength + windDrift;
    p.velocity.x += (desiredVx - p.velocity.x) * 6.5f * dt;

    // 強力な上昇気流で下から上へ巻き上げる
    float coreFactor = saturate(1.0f - absDx / max(pullRange, 1.0f));
    float upSpeed = lerp(260.0f, 880.0f, coreFactor) * lerp(0.75f, 1.35f, FixedRand(index, 21));
    if (layer == 0u)
    {
        upSpeed *= 0.65f;
    }
    float desiredVy = -upSpeed + sin(t * 4.0f + phase) * 55.0f;
    p.velocity.y += (desiredVy - p.velocity.y) * 5.0f * dt;

    // 奥行きに応じた明暗・サイズ感の演出
    if (layer >= 2u)
    {
        float depthScale = lerp(0.75f, 1.30f, depthZ * 0.5f + 0.5f);
        float baseGrain = lerp(2.8f, 6.8f, FixedRand(index, 16)) * depthScale;
        p.scale.x = baseGrain * 2.5f;
        p.scale.y = baseGrain;
        p.position.z = atan2(p.velocity.y, p.velocity.x);
    }
    else
    {
        p.position.z += swirlSpeed * 0.12f * dt;
    }
}

// スペースキー押下中の花吹雪のつむじ風演出（モード2:花びら）
void ApplyPetalTornado(inout ParticleForGPU p, uint index)
{
    float dt = gCSConstant.deltaTime;
    float t = gCSConstant.time;
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float phase = float(index % 997u);

    // 地面に積もった花びらも全て舞い上げる
    if (p.velocity.z > 0.5f)
    {
        p.velocity.z = 0.0f;
        p.position.y -= 6.0f + Rand(index, 35) * 12.0f;
        p.velocity.y = -lerp(120.0f, 280.0f, Rand(index, 36));
        p.position.z = lerp(gCSConstant.speedMin, gCSConstant.speedMax, FixedRand(index, 34));
    }

    // 画面中央付近を優雅に蛇行する花吹雪の渦
    float baseX = (gCSConstant.mouseX > -5000.0f) ? lerp(W * 0.5f, clamp(gCSConstant.mouseX, W * 0.15f, W * 0.85f), 0.4f) : W * 0.5f;
    float centerX = baseX + sin(t * 1.1f) * 180.0f + cos(t * 2.1f) * 60.0f;
    float hNorm = saturate((H - p.position.y) / max(H, 1.0f));
    float funnelRadius = lerp(55.0f, 440.0f, pow(hNorm, 0.95f));
    float spineX = centerX + sin(hNorm * 4.5f - t * 3.4f) * (45.0f * hNorm + 12.0f);

    float swirlSpeed = lerp(7.5f, 4.2f, hNorm) + FixedRand(index, 37) * 2.5f;
    float swirlAngle = t * swirlSpeed + phase * 0.53f + hNorm * 5.0f;
    float orbitRadius = funnelRadius * lerp(0.15f, 1.05f, FixedRand(index, 38));
    float targetX = spineX + cos(swirlAngle) * orbitRadius;

    float desiredVx = (targetX - p.position.x) * 4.5f + gCSConstant.wind * (0.25f + 0.7f * hNorm);
    p.velocity.x += (desiredVx - p.velocity.x) * 4.2f * dt;

    float upSpeed = lerp(160.0f, 460.0f, FixedRand(index, 39));
    float desiredVy = -upSpeed + sin(t * 3.2f + phase) * 65.0f;
    p.velocity.y += (desiredVy - p.velocity.y) * 3.5f * dt;
}

// スペースキー押下中の暴風竜巻＆雷撃演出（モード3:嵐の夜）
void ApplyStormTornado(inout ParticleForGPU p, uint index, uint layer)
{
    float dt = gCSConstant.deltaTime;
    float t = gCSConstant.time;
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float phase = float(index % 997u);

    // 稲妻・放電パーティクル（全体の約3.5%を雷撃として空を走らせる）
    if (index % 28u == 0u)
    {
        uint boltGroup = (index / 28u) % 4u;
        float boltCycle = frac(t * 2.6f + float(boltGroup) * 0.37f);
        float boltSlot = floor(t * 2.6f + float(boltGroup) * 0.37f);
        float segNorm = frac(float(index / 112u) * 0.037f);

        float strikeX = W * lerp(0.18f, 0.82f, Hash(uint(boltSlot) * 733u + boltGroup * 97u));
        float zigzag = sin(segNorm * 22.0f + boltSlot * 5.1f) * 34.0f + cos(segNorm * 47.0f - boltSlot * 3.7f) * 14.0f;
        p.position.x = strikeX + zigzag * segNorm + (Rand(index, 41) - 0.5f) * 12.0f;
        p.position.y = segNorm * H * 0.78f;
        p.velocity = float3(0.0f, 0.0f, 0.0f);

        // 稲妻が走って一瞬で消えるフラッシュ
        float flashEnv = saturate(1.0f - boltCycle * 3.2f) * step(segNorm, saturate(boltCycle * 6.0f));
        float boltW = lerp(18.0f, 6.0f, segNorm) * (0.6f + flashEnv * 0.8f);
        p.scale = float3(boltW, 38.0f, flashEnv);
        p.color = float4(0.75f, 0.88f, 1.0f, flashEnv);
        p.pad = 7.0f + FixedRand(index, 9) * 0.45f;
        p.currentTime = p.lifeTime * 0.5f;
        return;
    }

    // 暴風竜巻（ウォータースパウト）の渦巻き
    float baseX = (gCSConstant.mouseX > -5000.0f) ? lerp(W * 0.5f, clamp(gCSConstant.mouseX, W * 0.15f, W * 0.85f), 0.35f) : W * 0.5f;
    float centerX = baseX + sin(t * 1.6f) * 170.0f + sin(t * 3.4f) * 55.0f;
    float hNorm = saturate((H - p.position.y) / max(H, 1.0f));
    float funnelRadius = lerp(45.0f, 380.0f, pow(hNorm, 1.1f));
    float spineX = centerX + sin(hNorm * 6.0f - t * 5.8f) * (38.0f * hNorm + 10.0f);

    float swirlSpeed = lerp(14.0f, 8.0f, hNorm) + FixedRand(index, 42) * 4.0f;
    float swirlAngle = t * swirlSpeed + phase * 0.37f + hNorm * 7.2f;
    float orbitRadius = funnelRadius * lerp(0.10f, 1.08f, FixedRand(index, 43));
    float targetX = spineX + cos(swirlAngle) * orbitRadius;

    float desiredVx = (targetX - p.position.x) * 8.5f + gCSConstant.wind * 0.35f;
    p.velocity.x += (desiredVx - p.velocity.x) * 7.5f * dt;

    float upSpeed = lerp(340.0f, 960.0f, FixedRand(index, 44));
    if (layer == 0u)
    {
        upSpeed *= 0.6f;
    }
    p.velocity.y += (-upSpeed - p.velocity.y) * 6.0f * dt;

    // 渦の中では水しぶき・霧として激しい暴風を表現
    if (p.pad >= 4.5f && p.pad < 5.5f)
    {
        p.pad = 4.0f + FixedRand(index, 9) * 0.45f;
        p.scale = float3(10.0f, 10.0f, 0.85f);
    }
}

// スペースキー押下中のオーロラ降臨＆無重力ダイヤモンドダスト演出（モード4:雪景色）
void ApplySnowAuroraAndDiamondDust(inout ParticleForGPU p, uint index, uint layer)
{
    float dt = gCSConstant.deltaTime;
    float t = gCSConstant.time;
    float W = gCSConstant.screenW;
    float H = gCSConstant.screenH;
    float phase = float(index % 997u);

    // オーロラカーテン（全体の約18%を夜空にゆらめく極光のベールにする）
    if (index % 11u < 2u)
    {
        uint ribbonId = index % 2u; // 2重のオーロラ帯
        float u = FixedRand(index, 61);
        float xPos = lerp(-60.0f, W + 60.0f, u);

        // 空間的に連続した波打ち（オーロラのカーテンのうねり）
        float wave1 = sin(u * 7.5f + t * 0.85f + float(ribbonId) * 1.9f) * 52.0f;
        float wave2 = cos(u * 15.0f - t * 1.35f + float(ribbonId) * 0.8f) * 22.0f;
        float baseY = (ribbonId == 0u) ? H * 0.24f : H * 0.38f;
        float ribbonH = lerp(120.0f, 230.0f, FixedRand(index, 62)) * (0.85f + 0.25f * sin(u * 9.0f + t * 1.6f));

        p.position.x = xPos + sin(t * 0.9f + u * 6.0f) * 18.0f;
        p.position.y = baseY + wave1 + wave2 + (FixedRand(index, 63) - 0.5f) * 28.0f;
        p.velocity = float3(0.0f, 0.0f, 0.0f);

        // エメラルドグリーン〜シアン〜バイオレットへ移ろう極光カラー
        float shimmer = 0.5f + 0.5f * sin(u * 12.0f - t * 2.2f + float(ribbonId) * 2.4f);
        float3 auroraGreen = float3(0.18f, 0.96f, 0.64f);
        float3 auroraCyan = float3(0.28f, 0.78f, 1.00f);
        float3 auroraViolet = float3(0.68f, 0.36f, 0.98f);
        float3 col = lerp(auroraGreen, auroraCyan, shimmer);
        col = lerp(col, auroraViolet, pow(saturate(sin(u * 5.0f + t * 0.7f)), 2.0f) * 0.65f);

        float curtainW = lerp(22.0f, 44.0f, FixedRand(index, 64));
        float alpha = (0.24f + 0.18f * shimmer) * (ribbonId == 0u ? 1.0f : 0.75f);
        p.scale = float3(curtainW, ribbonH, alpha);
        p.color = float4(col, 1.0f);
        p.position.z = sin(u * 6.0f + t * 0.8f) * 0.14f; // カーテンのわずかな傾き
        p.pad = 11.0f + FixedRand(index, 9) * 0.45f; // 11.x: オーロラシェーダー
        p.currentTime = p.lifeTime * 0.5f;
        return;
    }

    // 地面に積もっていた雪も静かに無重力浮遊を開始する
    if (p.velocity.z > 0.5f)
    {
        p.velocity.z = 0.0f;
        p.position.y -= 3.0f + Rand(index, 65) * 6.0f;
        p.velocity.y = -lerp(18.0f, 55.0f, Rand(index, 66));
        p.currentTime = min(p.currentTime, p.lifeTime * 0.35f);
    }

    // 無重力でゆっくりと天へ昇りながら左右にたゆたう
    float driftX = sin(t * 1.15f + phase * 0.37f) * 32.0f + cos(t * 0.65f + p.position.y * 0.01f) * 18.0f;
    float riseSpeed = lerp(24.0f, 78.0f, FixedRand(index, 67));
    if (layer == 0u)
    {
        riseSpeed *= 0.6f;
    }
    float desiredVy = -riseSpeed + sin(t * 1.8f + phase) * 14.0f;
    p.velocity.x += (driftX - p.velocity.x) * 2.2f * dt;
    p.velocity.y += (desiredVy - p.velocity.y) * 2.5f * dt;

    // 一部の雪粒をキラキラ輝くダイヤモンドダスト（十字の光の粒）へ変化させる
    if (index % 3u == 0u && layer >= 1u)
    {
        float sparkleSize = lerp(9.0f, 20.0f, FixedRand(index, 68));
        p.scale = float3(sparkleSize, sparkleSize, 0.95f);
        p.color.rgb = lerp(float3(0.82f, 0.95f, 1.0f), float3(1.0f, 0.98f, 0.88f), FixedRand(index, 69));
        p.pad = 10.0f + FixedRand(index, 9) * 0.45f; // 10.x: ダイヤモンドダストの十字光
    }
}

// 粒の(再)生成。座標は画面ピクセル(左上原点)。
// scale.z = 基準アルファ, position.z = 粒ごとの固有値, velocity.z = 堆積状態(0:飛翔 / 1:堆積)
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
    p.velocity.z = 0.0f;

    if (gCSConstant.mode == 1) // 砂漠・砂嵐: 4層構造で砂煙・飛翔砂・堆積砂を表現
    {
        uint layer = GetDesertLayer(index);

        if (layer == 0u)
        {
            // 層0: 遠景〜中景を覆う巨大な砂煙（柔らかく薄い）
            float s = lerp(gCSConstant.sizeMax * 1.4f, gCSConstant.sizeMax * 2.8f, Rand(index, 1));
            p.position.xy = float2(fill ? rx * (W + s) - s * 0.5f : -s * 0.6f, ry * H);
            p.velocity = float3(speed * lerp(0.35f, 0.55f, Rand(index, 7)), (Rand(index, 8) - 0.5f) * 35.0f, 0.0f);
            p.scale = float3(s * 1.25f, s * 0.9f * gCSConstant.stretch, col.a * 0.28f);
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
            p.scale = float3(s * 1.6f, s * 0.65f * gCSConstant.stretch, col.a * 0.50f);
            p.position.z = (Rand(index, 10) - 0.5f) * 0.5f;
            p.pad = 1.0f + grainSeed * 0.45f; // 1.0~1.45: 柔らかい砂煙シェーダー
        }
        else if (layer == 2u)
        {
            // 層2: 空中を吹き飛び、地面に落ちると積もる砂粒
            float yBias = pow(ry, 0.68f);
            float grainSize = lerp(2.4f, 6.8f, Rand(index, 1) * Rand(index, 1));
            float fastSpeed = speed * lerp(1.15f, 2.15f, Rand(index, 7));
            float streak = clamp(fastSpeed / 260.0f, 1.5f, 5.5f) * gCSConstant.stretch;
            p.position.xy = float2(fill ? rx * W : -grainSize * streak, yBias * H);
            p.velocity = float3(fastSpeed, (Rand(index, 8) - 0.55f) * fastSpeed * 0.22f, 0.0f);
            p.scale = float3(grainSize * streak, grainSize, min(col.a * 1.35f, 0.95f));
            p.position.z = 0.0f;
            p.color.rgb = lerp(p.color.rgb, float3(1.0f, 0.92f, 0.76f), 0.35f);
            p.pad = 1.52f + grainSeed * 0.45f; // 1.52~1.97: 砂粒シェーダー
        }
        else
        {
            // 層3: 地面に貯まっている砂場（砂丘の層を形成）
            float xPos = rx * (W + 40.0f) - 20.0f;
            float depositY = GetParticleDepositY(index, xPos, W, H);
            float clumpW = lerp(14.0f, 34.0f, Rand(index, 1));
            float clumpH = clumpW * lerp(0.38f, 0.62f, Rand(index, 7));
            p.position.xy = float2(xPos, depositY);
            p.velocity = float3(0.0f, 0.0f, 1.0f); // 1.0 = 地面に堆積中
            p.scale = float3(clumpW, clumpH, min(col.a * 1.45f, 0.92f));
            p.position.z = (Rand(index, 10) - 0.5f) * 0.25f;
            p.lifeTime = lerp(18.0f, 35.0f, Rand(index, 6));
            // 地表の砂は深さに応じて陰影をつける
            float depthShade = lerp(1.05f, 0.82f, FixedRand(index, 13));
            p.color.rgb = lerp(p.color.rgb, float3(0.95f, 0.83f, 0.60f), 0.4f) * depthShade;
            p.pad = 1.52f + grainSeed * 0.45f;
        }
    }
    else if (gCSConstant.mode == 2) // 花びら: 遠景光粒・舞う花びら・地面の絨毯の3層構造
    {
        uint layer = GetPetalLayer(index);
        float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));

        if (layer == 0u)
        {
            // 層0: 春の陽光のボケ光と遠景の大きな花びら
            bool isBokeh = (index % 3u != 0u);
            float s = isBokeh ? lerp(18.0f, 52.0f, Rand(index, 1)) : size * 1.45f;
            p.position.xy = float2(rx * (W + 300.0f) - 200.0f, fill ? ry * H : -s * 2.0f);
            p.velocity = float3(gCSConstant.wind * 0.6f, speed * 0.55f, 0.0f);
            p.scale = float3(s, isBokeh ? s : s * gCSConstant.stretch, isBokeh ? col.a * 0.26f : col.a * 0.45f);
            p.position.z = speed * 0.55f;
            p.color.rgb = isBokeh ? float3(1.0f, 0.93f, 0.96f) : col.rgb;
            p.pad = isBokeh ? (2.55f + grainSeed * 0.4f) : (2.05f + grainSeed * 0.4f);
        }
        else if (layer == 1u)
        {
            // 層1: 空中をひらひら舞い落ちる花びら
            p.position.xy = float2(rx * (W + 350.0f) - 300.0f, fill ? ry * H : -size * 2.0f);
            p.velocity = float3(gCSConstant.wind, speed, 0.0f);
            p.scale = float3(size, size * gCSConstant.stretch, col.a);
            p.position.z = speed;
            p.pad = 2.05f + grainSeed * 0.4f;
        }
        else
        {
            // 層2: 地面に積もっている花びらの絨毯
            float xPos = rx * (W + 40.0f) - 20.0f;
            float groundY = GetPetalGroundY(index, xPos, W, H);
            float petalW = size * lerp(0.85f, 1.25f, Rand(index, 7));
            p.position.xy = float2(xPos, groundY);
            p.velocity = float3(0.0f, 0.0f, 1.0f); // 1.0 = 地面に堆積中
            p.scale = float3(petalW, petalW * gCSConstant.stretch * 0.72f, col.a * 0.92f);
            p.position.z = (Rand(index, 10) - 0.5f) * 1.4f; // 堆積時の向き
            p.lifeTime = lerp(16.0f, 32.0f, Rand(index, 6));
            p.pad = 2.05f + grainSeed * 0.4f;
        }
    }
    else if (gCSConstant.mode == 3) // 嵐: 霧・霧雨・豪雨の3層構造
    {
        uint layer = GetStormLayer(index);
        float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));

        if (layer == 0u)
        {
            // 層0: 低空を流れる暗い嵐の霧・雨雲
            float fogS = lerp(140.0f, 320.0f, Rand(index, 1));
            p.position.xy = float2(fill ? rx * (W + fogS) - fogS * 0.5f : -fogS * 0.6f, lerp(H * 0.25f, H, ry));
            p.velocity = float3(gCSConstant.wind * 2.2f + 160.0f, (Rand(index, 7) - 0.5f) * 25.0f, 0.0f);
            p.scale = float3(fogS * 1.6f, fogS * 0.75f, 0.16f);
            p.color = float4(0.24f, 0.30f, 0.42f, 1.0f);
            p.position.z = Rand(index, 10) * 6.283185f;
            p.pad = 6.0f + grainSeed * 0.45f;
        }
        else if (layer == 1u)
        {
            // 層1: 奥の細かい霧雨
            float s = size * 0.65f;
            p.scale = float3(s, s * gCSConstant.stretch * 0.85f, col.a * 0.45f);
            p.position.xy = float2(rx * (W + 400.0f) - 200.0f, fill ? ry * H : -p.scale.y);
            p.velocity = float3(gCSConstant.wind * 0.85f, speed * 0.8f, 0.0f);
            p.position.z = H - Rand(index, 7) * 160.0f - 20.0f;
            p.pad = 3.0f + grainSeed * 0.45f;
        }
        else
        {
            // 層2: 手前の大粒の豪雨
            p.scale = float3(size, size * gCSConstant.stretch, col.a);
            p.position.xy = float2(rx * (W + 400.0f) - 200.0f, fill ? ry * H : -p.scale.y);
            p.velocity = float3(gCSConstant.wind, speed, 0.0f);
            p.position.z = H - Rand(index, 7) * 130.0f - 8.0f; // 着地ライン
            p.pad = 3.0f + grainSeed * 0.45f;
        }
    }
    else // mode 4: 雪景色 (遠景ボケ雪・舞い落ちる雪の結晶＆綿雪・銀世界の積雪)
    {
        uint layer = GetSnowLayer(index);
        float size = lerp(gCSConstant.sizeMin, gCSConstant.sizeMax, Rand(index, 1));
        float3 snowTint = lerp(float3(0.86f, 0.94f, 1.0f), float3(1.0f, 1.0f, 1.0f), Rand(index, 3));

        if (layer == 0u)
        {
            // 層0: 遠景をゆっくり漂う淡いボケ雪と冷気
            float s = lerp(16.0f, 46.0f, Rand(index, 1));
            p.position.xy = float2(rx * (W + 300.0f) - 150.0f, fill ? ry * H : -s * 2.0f);
            p.velocity = float3(gCSConstant.wind * 0.45f, speed * 0.48f, 0.0f);
            p.scale = float3(s, s, col.a * 0.25f);
            p.position.z = speed * 0.48f;
            p.color.rgb = float3(0.84f, 0.93f, 1.0f);
            p.pad = 9.0f + grainSeed * 0.45f; // 9.x: 柔らかい綿雪・ボケ雪シェーダー
        }
        else if (layer == 1u)
        {
            // 層1: 空中を静かに舞い降りる雪の結晶（六角形）とふわふわ綿雪
            bool isCrystal = (index % 3u == 0u);
            float s = isCrystal ? size * 1.35f : size * lerp(0.7f, 1.15f, Rand(index, 7));
            p.position.xy = float2(rx * (W + 320.0f) - 160.0f, fill ? ry * H : -s * 2.0f);
            p.velocity = float3(gCSConstant.wind, speed, 0.0f);
            p.scale = float3(s, s, isCrystal ? min(col.a * 1.15f, 0.96f) : col.a * 0.85f);
            p.position.z = speed;
            p.color.rgb = snowTint;
            p.pad = isCrystal ? (8.0f + grainSeed * 0.45f) : (9.0f + grainSeed * 0.45f);
        }
        else
        {
            // 層2: 地面に積もる雪原（銀世界を形成）
            float xPos = rx * (W + 40.0f) - 20.0f;
            float groundY = GetSnowGroundY(index, xPos, W, H);
            float clumpW = lerp(14.0f, 32.0f, Rand(index, 1));
            float clumpH = clumpW * lerp(0.42f, 0.68f, Rand(index, 7));
            p.position.xy = float2(xPos, groundY);
            p.velocity = float3(0.0f, 0.0f, 1.0f); // 1.0 = 地面に堆積中
            p.scale = float3(clumpW, clumpH, min(col.a * 1.1f, 0.92f));
            p.position.z = (Rand(index, 10) - 0.5f) * 0.25f;
            p.lifeTime = lerp(18.0f, 34.0f, Rand(index, 6));
            float depthShade = lerp(1.02f, 0.80f, FixedRand(index, 51));
            p.color.rgb = lerp(float3(0.82f, 0.91f, 1.0f), float3(1.0f, 1.0f, 1.0f), 0.55f) * depthShade;
            p.pad = 9.0f + grainSeed * 0.45f;
        }
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
        bool isTornado = gCSConstant.tornado > 0.5f;

        // スペースキー押下中: 竜巻による渦巻き上昇気流
        if (isTornado)
        {
            ApplyTornado(p, index, layer);
            if (p.velocity.z > 0.5f)
            {
                // 竜巻圏外の堆積砂でもマウス移動には反応する
                ApplyMouseToSettledSand(p, index);
                return false;
            }
            p.position.xy += p.velocity.xy * dt;
            return p.position.y < -220.0f || p.position.x < -350.0f || p.position.x > W + 350.0f;
        }

        // 地面に貯まっている砂の処理
        if (p.velocity.z > 0.5f)
        {
            // マウスが砂場で移動すると砂が反応して舞い上がる
            ApplyMouseToSettledSand(p, index);
            if (p.velocity.z > 0.5f)
            {
                // 堆積中は寿命の進行を緩やかにして砂場を長く維持する
                p.currentTime -= dt * 0.75f;
                return false;
            }
        }

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
            // 砂粒・舞い上がった砂: 風と重力で飛び、地面に着地すると砂場に貯まる
            float targetVx = gCSConstant.wind * 1.45f * gust + airFlow.x * gCSConstant.turbulence * 1.5f;
            p.velocity.x += (targetVx - p.velocity.x) * 2.0f * dt;
            p.velocity.y += (240.0f + airFlow.y * gCSConstant.turbulence * 2.0f) * dt; // 重力＋乱流
            ApplyMouse(p, 0.0f, true);
            p.position.xy += p.velocity.xy * dt;

            // 砂丘の堆積高さに到達したときの着地判定
            float depositY = GetParticleDepositY(index, p.position.x, W, H);
            if (p.position.y >= depositY && p.velocity.y > 0.0f)
            {
                // 画面内の砂場であれば一定確率で地面に貯まる（残りは跳ねて飛ぶ）
                bool insideScreen = (p.position.x >= -10.0f && p.position.x <= W + 10.0f);
                if (insideScreen && (layer == 3u || Rand(index, 22) < 0.55f))
                {
                    p.position.y = depositY;
                    p.velocity = float3(0.0f, 0.0f, 1.0f); // 地面に堆積
                    float clumpW = lerp(12.0f, 28.0f, FixedRand(index, 23));
                    p.scale = float3(clumpW, clumpW * 0.48f, 0.86f);
                    p.position.z = (FixedRand(index, 24) - 0.5f) * 0.25f;
                    p.currentTime = 0.0f;
                    p.lifeTime = lerp(16.0f, 30.0f, Rand(index, 25));
                    return false;
                }
                else
                {
                    p.position.y = depositY;
                    p.velocity.y = -abs(p.velocity.y) * lerp(0.30f, 0.65f, Rand(index, 12)) - 35.0f;
                    p.velocity.x *= 0.85f;
                }
            }
            return p.position.x > W + 140.0f || p.position.x < -140.0f || p.position.y < -180.0f;
        }
    }

    if (gCSConstant.mode == 2)
    {
        uint layer = GetPetalLayer(index);
        bool isTornado = gCSConstant.tornado > 0.5f;

        // スペースキー押下中: 桜のつむじ風（花吹雪）
        if (isTornado)
        {
            ApplyPetalTornado(p, index);
            p.position.xy += p.velocity.xy * dt;
            return p.position.y < -180.0f || p.position.x < -300.0f || p.position.x > W + 300.0f;
        }

        // 地面に積もった花びらの絨毯の処理
        if (p.velocity.z > 0.5f)
        {
            ApplyMouseToSettledPetals(p, index);
            if (p.velocity.z > 0.5f)
            {
                p.currentTime -= dt * 0.75f;
                return false;
            }
        }

        // 気流とひらひら舞う横揺れ
        float2 airFlow = SampleAirFlow(p.position.xy, t * 0.6f);
        float sway = sin(t * 1.8f + phase) * gCSConstant.turbulence + airFlow.x * 35.0f;
        float floatLift = cos(t * 2.4f + phase * 1.3f) * 28.0f;
        p.velocity.x += (gCSConstant.wind + sway - p.velocity.x) * 2.0f * dt;
        p.velocity.y += (p.position.z + floatLift - p.velocity.y) * 1.6f * dt;
        ApplyMouse(p, 0.75f, false);
        p.position.xy += p.velocity.xy * dt;

        // 空中の花びらが地面に着地したら一定確率で絨毯として積もる
        if (layer >= 1u && p.pad < 2.5f && p.velocity.y > 0.0f)
        {
            float groundY = GetPetalGroundY(index, p.position.x, W, H);
            if (p.position.y >= groundY)
            {
                bool insideScreen = (p.position.x >= -10.0f && p.position.x <= W + 10.0f);
                if (insideScreen && (layer == 2u || Rand(index, 37) < 0.45f))
                {
                    p.position.y = groundY;
                    p.velocity = float3(0.0f, 0.0f, 1.0f);
                    p.position.z = (FixedRand(index, 38) - 0.5f) * 1.4f;
                    p.currentTime = 0.0f;
                    p.lifeTime = lerp(16.0f, 30.0f, Rand(index, 39));
                    return false;
                }
                return true;
            }
        }
        return p.position.y > H + 50.0f || p.position.x > W + 300.0f || p.position.x < -300.0f;
    }

    if (gCSConstant.mode == 3)
    {
        // mode 3: 嵐の夜 (pad 3:雨 / 4:水しぶき / 5:水たまり波紋 / 6:霧 / 7:稲妻)
        bool isStormTornado = gCSConstant.tornado > 0.5f;
        if (isStormTornado)
        {
            uint layer = GetStormLayer(index);
            ApplyStormTornado(p, index, layer);
            if (p.pad < 6.5f)
            {
                p.position.xy += p.velocity.xy * dt;
                return p.position.y < -220.0f || p.position.x < -350.0f || p.position.x > W + 350.0f;
            }
            return false;
        }

        // スペースキーを離した直後の稲妻パーティクルは雨に戻す
        if (p.pad >= 6.5f)
        {
            return true;
        }

        if (p.pad >= 5.5f)
        {
            // 霧・雨雲の流れ
            p.velocity.x = gCSConstant.wind * 2.2f + 160.0f + sin(t * 0.7f + phase) * 35.0f;
            p.position.xy += p.velocity.xy * dt;
            float margin = p.scale.x * 0.8f + 100.0f;
            return p.position.x > W + margin;
        }

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
                if (hitGround && !hitMouse && (index % 4u == 0u))
                {
                    // 地面に着地した一部の雨粒は広がる水たまりの波紋になる
                    p.pad = 5.0f + FixedRand(index, 9) * 0.45f;
                    p.currentTime = 0.0f;
                    p.lifeTime = 0.45f + Rand(index, 8) * 0.25f;
                    p.velocity = float3(0.0f, 0.0f, 0.0f);
                    p.scale = float3(10.0f, 4.0f, 0.65f);
                    p.color = float4(0.75f, 0.85f, 1.0f, 1.0f);
                }
                else
                {
                    // 跳ね返る水しぶき
                    p.pad = 4.0f + FixedRand(index, 9) * 0.45f;
                    p.currentTime = 0.0f;
                    p.lifeTime = 0.3f + Rand(index, 8) * 0.3f;
                    p.velocity = float3((Rand(index, 9) - 0.5f) * 260.0f, -(80.0f + Rand(index, 10) * 260.0f), 0.0f);
                    float s = hitMouse ? 12.0f : 7.0f;
                    p.scale = float3(s, s, 0.9f);
                    p.color = float4(0.8f, 0.9f, 1.0f, 1.0f);
                }
            }
        }
        else if (p.pad < 4.5f)
        {
            // 水しぶきの放物線運動
            p.velocity.y += 900.0f * dt;
            p.position.xy += p.velocity.xy * dt;
        }
        else
        {
            // 波紋リングの拡大
            p.scale.x += 95.0f * dt;
            p.scale.y += 38.0f * dt;
        }
        return false;
    }

    // mode 4: 雪景色 (pad 8:雪の結晶 / 9:綿雪・積雪 / 10:ダイヤモンドダスト / 11:オーロラ)
    uint snowLayer = GetSnowLayer(index);
    bool isAuroraSpecial = gCSConstant.tornado > 0.5f;

    // スペースキー押下中: オーロラ降臨＆無重力ダイヤモンドダスト演出
    if (isAuroraSpecial)
    {
        ApplySnowAuroraAndDiamondDust(p, index, snowLayer);
        if (p.pad < 10.5f)
        {
            p.position.xy += p.velocity.xy * dt;
            return p.position.y < -120.0f || p.position.x < -250.0f || p.position.x > W + 250.0f;
        }
        return false;
    }

    // スペースキーを離した直後のオーロラ・ダイヤモンドダストは通常の雪へ戻す
    if (p.pad >= 9.5f)
    {
        return true;
    }

    // 地面に積もった雪の処理
    if (p.velocity.z > 0.5f)
    {
        ApplyMouseToSettledSnow(p, index);
        if (p.velocity.z > 0.5f)
        {
            p.currentTime -= dt * 0.78f;
            return false;
        }
    }

    // 静かに舞い降りる雪の横揺れと気流
    float2 snowAir = SampleAirFlow(p.position.xy, t * 0.45f);
    float sway = sin(t * 1.35f + phase) * (gCSConstant.turbulence * 0.75f) + snowAir.x * 26.0f;
    float gentleFloat = cos(t * 1.9f + phase * 1.1f) * 14.0f;
    p.velocity.x += (gCSConstant.wind + sway - p.velocity.x) * 1.8f * dt;
    p.velocity.y += (p.position.z + gentleFloat - p.velocity.y) * 1.5f * dt;
    ApplyMouse(p, 0.55f, false);
    p.position.xy += p.velocity.xy * dt;

    // 空中の雪が地面に着地したら雪原として積もる
    if (snowLayer >= 1u && p.velocity.y > 0.0f)
    {
        float groundY = GetSnowGroundY(index, p.position.x, W, H);
        if (p.position.y >= groundY)
        {
            bool insideScreen = (p.position.x >= -10.0f && p.position.x <= W + 10.0f);
            if (insideScreen && (snowLayer == 2u || Rand(index, 57) < 0.50f))
            {
                p.position.y = groundY;
                p.velocity = float3(0.0f, 0.0f, 1.0f);
                float clumpW = lerp(13.0f, 28.0f, FixedRand(index, 58));
                p.scale = float3(clumpW, clumpW * 0.52f, 0.90f);
                p.position.z = (FixedRand(index, 59) - 0.5f) * 0.25f;
                p.pad = 9.0f + FixedRand(index, 9) * 0.45f;
                p.currentTime = 0.0f;
                p.lifeTime = lerp(18.0f, 32.0f, Rand(index, 60));
                return false;
            }
            return true;
        }
    }
    return p.position.y > H + 50.0f || p.position.x > W + 280.0f || p.position.x < -280.0f;
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
    float2 drawScale = p.scale.xy;

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
        drawScale = p.scale.xy;
    }
    else if (index >= gCSConstant.activeCount)
    {
        // Editorで count を減らした分は非表示(次に増えたとき lifeTime=0 で作り直される)
        p.currentTime = 0.0f;
        p.lifeTime = 0.0f;
        p.scale = float3(0.0f, 0.0f, 0.0f);
        drawScale = float2(0.0f, 0.0f);
    }
    else
    {
        // ===== モード1~4: 画面ピクセル座標で動く演出 =====
        bool fill = reset || p.lifeTime == 0.0f; // 画面全体にばらまいて作る
        p.currentTime += gCSConstant.deltaTime;

        bool respawn = fill || p.currentTime >= p.lifeTime;
        if (!respawn)
            respawn = Step(p, index);
        if (respawn)
            Emit(p, index, fill);

        drawScale = p.scale.xy;
        float t = p.currentTime;
        float fade;
        if (gCSConstant.mode == 4)
        {
            // 雪景色モード (pad 8:結晶 / 9:綿雪・積雪 / 10:ダイヤモンドダスト / 11:オーロラ)
            fade = saturate(min(t, p.lifeTime - t) / 0.6f);
            if (p.pad >= 10.5f)
            {
                // オーロラカーテンのゆらめき
                fade = 1.0f;
                rotAngle = p.position.z;
            }
            else if (p.pad >= 9.5f)
            {
                // ダイヤモンドダストの瞬き（キラッと明滅・回転）
                float twinkle = 0.45f + 0.55f * abs(sin(gCSConstant.time * 5.5f + float(index % 199u) * 0.31f));
                fade *= twinkle;
                rotAngle = gCSConstant.time * 1.4f + float(index % 314u) * 0.02f;
            }
            else if (p.velocity.z > 0.5f)
            {
                // 地面に積もった雪
                rotAngle = p.position.z;
            }
            else if (p.pad < 8.5f)
            {
                // 舞い降りる雪の結晶はゆっくり回転しながらきらめく
                float dirSign = (index % 2u == 0u) ? 1.0f : -1.0f;
                rotAngle = gCSConstant.time * 1.25f * dirSign + float(index % 628u) * 0.01f;
            }
        }
        else if (gCSConstant.mode == 3)
        {
            if (p.pad >= 6.5f)
            {
                // 稲妻の電撃閃光
                fade = 1.0f;
                rotAngle = sin(gCSConstant.time * 40.0f + float(index)) * 0.35f;
            }
            else if (p.pad >= 5.5f)
            {
                // 霧の滑らかなフェード
                fade = saturate(min(t, p.lifeTime - t) / 1.2f);
                rotAngle = p.position.z;
            }
            else if (p.pad >= 4.5f)
            {
                // 波紋リングのフェードアウト
                fade = 1.0f - (t / p.lifeTime);
            }
            else if (p.pad >= 3.5f)
            {
                // 水しぶきのフェードアウト
                fade = 1.0f - (t / p.lifeTime);
            }
            else
            {
                // 雨粒（竜巻中は進行方向へ傾く）
                fade = 1.0f;
                rotAngle = (gCSConstant.tornado > 0.5f)
                    ? atan2(p.velocity.x, max(abs(p.velocity.y), 1.0f)) * 0.65f
                    : atan2(p.velocity.x, max(p.velocity.y, 1.0f)) * -0.5f;
            }
        }
        else if (gCSConstant.mode == 2)
        {
            fade = saturate(min(t, p.lifeTime - t) / 0.5f);
            if (p.velocity.z > 0.5f)
            {
                // 地面に積もった花びら
                rotAngle = p.position.z;
            }
            else if (p.pad >= 2.5f)
            {
                // 春の陽光ボケ光（ふわっと明滅）
                float twinkle = 0.65f + 0.35f * sin(gCSConstant.time * 3.0f + float(index % 313u));
                fade *= twinkle;
            }
            else
            {
                // 舞う花びら: 回転＋3D風の裏返りフリップ
                float spinSpeed = (gCSConstant.tornado > 0.5f) ? 4.8f : 2.2f;
                rotAngle = gCSConstant.time * spinSpeed + float(index % 628u) * 0.01f;
                float flip = 0.35f + 0.65f * abs(cos(gCSConstant.time * 4.5f + float(index % 251u) * 0.13f));
                drawScale.y *= flip;
            }
        }
        else
        {
            // 砂漠モード: 出現・消滅を滑らかにフェード
            fade = saturate(min(t, p.lifeTime - t) / 0.8f);
            uint layer = GetDesertLayer(index);
            if (p.velocity.z > 0.5f)
            {
                rotAngle = p.position.z;
            }
            else if (layer >= 2u)
            {
                // 飛翔中の砂粒は進行方向に傾けてスピード感を出す
                rotAngle = atan2(p.velocity.y, p.velocity.x);
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
        drawScale.x, 0, 0, 0,
        0, drawScale.y, 0, 0,
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