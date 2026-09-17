#include "Common.hlsli"

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD0;
};

PhongPSIn main(VSIn i)
{
    PhongPSIn o;
    float4 wp = mul(float4(i.pos, 1.0), gWorld);
    o.wpos = wp.xyz;
    o.pos = mul(wp, gViewProj);
    o.nrm = normalize(mul(i.nrm, (float3x3)gWorld));
    o.uv = i.uv * gMaterial.w;
    o.color = gColor;
    return o;
}
