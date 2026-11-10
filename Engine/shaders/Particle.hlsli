struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    float4 color : COLOR0;
};

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
};

struct Material
{
    float4 color;
    uint enableLighting;
    float4x4 uvTransform;
};

// GPU側でデータ更新と描画用データを統一して保持する構造体
struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float3 position;
    float currentTime;
    float3 velocity;
    float lifeTime;
    float3 scale;
    float pad;
};