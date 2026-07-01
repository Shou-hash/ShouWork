struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
};

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding; // パディングを追加して16バイト境界に揃える
    float32_t4x4 uvTransform;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
};