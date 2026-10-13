struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
    float32_t4 color : COLOR0; // ★ インスタンスカラー (αフェードアウト含む)
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
};

struct Material
{
    float32_t4 color;
    uint32_t enableLighting;
    float32_t4x4 uvTransform;
};

// ★ インスタンシング用データ構造体 (GPU転送用)
struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
};