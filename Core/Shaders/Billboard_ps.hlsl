Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct PSIn
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSIn i) : SV_Target
{
    float4 c = gTex.Sample(gSamp, i.uv) * i.color;
    clip(c.a - 0.02);
    return c;
}
