// Video-feedback step for TrailBuffer: samples the previous frame zoomed about the centre,
// optionally box-blurred, and attenuated.
Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer Feedback : register(b0)
{
    float4 gParams;   // x = fade, y = zoom (1 = none), z/w = blur step in uv (0 = none)
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 uv = (i.uv - 0.5) / gParams.y + 0.5;
    float2 d = gParams.zw;
    float4 c = gTex.Sample(gSamp, uv);
    if (d.x > 0.0 || d.y > 0.0)
    {
        c += gTex.Sample(gSamp, uv + float2(d.x, d.y));
        c += gTex.Sample(gSamp, uv + float2(-d.x, d.y));
        c += gTex.Sample(gSamp, uv + float2(d.x, -d.y));
        c += gTex.Sample(gSamp, uv + float2(-d.x, -d.y));
        c *= 0.2;
    }
    return c * gParams.x;
}
