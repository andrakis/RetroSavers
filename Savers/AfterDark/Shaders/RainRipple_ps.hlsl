// Rain over the desktop: up to 32 expanding rings refract the captured desktop.
Texture2D gDesktop : register(t0);
SamplerState gSamp : register(s0);

cbuffer Ripples : register(b0)
{
    float4 gInfo;          // x = ring count, y = aspect (w/h), z = has desktop, w = unused
    float4 gRings[32];     // x, y = centre (uv), z = radius (uv, vertical units), w = strength 0..1
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 offset = 0.0;
    float glint = 0.0;
    int n = (int)gInfo.x;
    for (int k = 0; k < 32; ++k)
    {
        if (k >= n) break;
        float4 r = gRings[k];
        float2 d = (i.uv - r.xy) * float2(gInfo.y, 1.0);
        float dist = length(d);
        float band = exp(-pow((dist - r.z) / 0.012, 2.0));
        float2 dir = dist > 1e-4 ? d / dist : 0.0;
        offset += dir * band * r.w * 0.012 * sin((dist - r.z) * 400.0);
        glint += band * r.w * 0.35;
    }
    float3 c;
    if (gInfo.z > 0.5) c = gDesktop.Sample(gSamp, i.uv + offset / float2(gInfo.y, 1.0)).rgb;
    else c = float3(0.02, 0.03, 0.06) + float3(0.1, 0.14, 0.2) * (1.0 - i.uv.y);
    c += glint * float3(0.8, 0.9, 1.0);
    return float4(c, 1.0);
}
