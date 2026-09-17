cbuffer Fill : register(b0)
{
    float4 gColor;
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    return gColor;
}
