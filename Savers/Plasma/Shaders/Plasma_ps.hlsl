// Demoscene plasma: a sum of sines mapped through a cycling cosine palette.
cbuffer Plasma : register(b0)
{
    float4 gTimeRes;   // x = time, y = width, z = height, w = scale
    float4 gPalA;      // cosine palette: colour = a + b * cos(2pi (c t + d))
    float4 gPalB;
    float4 gPalC;
    float4 gPalD;
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float t = gTimeRes.x;
    float aspect = gTimeRes.y / gTimeRes.z;
    float2 p = (i.uv - 0.5) * float2(aspect, 1.0) * gTimeRes.w;
    float v = sin(p.x * 3.1 + t * 0.9);
    v += sin(p.y * 2.7 - t * 1.1);
    v += sin((p.x + p.y) * 2.2 + t * 0.6);
    float2 c = p + float2(sin(t * 0.37) * 0.6, cos(t * 0.29) * 0.6);
    v += sin(sqrt(dot(c, c) + 0.05) * 4.5 - t * 1.4);
    v = v * 0.125 + 0.5;                         // 0..1
    float3 col = gPalA.rgb + gPalB.rgb * cos(6.2831853 * (gPalC.rgb * (v + t * 0.05) + gPalD.rgb));
    return float4(saturate(col), 1.0);
}
