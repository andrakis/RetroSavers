// Two photos with per-image scale/offset (fit / fill / Ken Burns) blended by a transition.
Texture2D gA : register(t0);
Texture2D gB : register(t1);
SamplerState gSamp : register(s0);

cbuffer Photo : register(b0)
{
    float4 gXformA;   // x, y = uv scale about the centre, z, w = uv offset
    float4 gXformB;
    float4 gMix;      // x = transition 0..1, y = mode (0 crossfade, 1 through black), z = has A, w = has B
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 Photo(Texture2D tex, float2 uv, float4 xf)
{
    float2 p = (uv - 0.5) * xf.xy + 0.5 + xf.zw;
    float inside = (any(p < 0.0) || any(p > 1.0)) ? 0.0 : 1.0;   // letterbox / pillarbox
    return tex.Sample(gSamp, saturate(p)).rgb * inside;
}

float4 main(PSIn i) : SV_Target
{
    float3 a = Photo(gA, i.uv, gXformA) * (gMix.z > 0.5 ? 1.0 : 0.0);
    float3 b = Photo(gB, i.uv, gXformB) * (gMix.w > 0.5 ? 1.0 : 0.0);
    float t = gMix.x;
    float3 c;
    if (gMix.y < 0.5) c = lerp(a, b, t);
    else c = a * (1.0 - saturate(t * 2.0)) + b * saturate(t * 2.0 - 1.0);
    return float4(c, 1.0);
}
