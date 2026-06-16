struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL;
};