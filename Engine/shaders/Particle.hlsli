struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    float4 color : COLOR0; // ★ インスタンスカラー (αフェードアウト含む)
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

// ★ インスタンシング用データ構造体 (GPU転送用)
struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};