struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
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