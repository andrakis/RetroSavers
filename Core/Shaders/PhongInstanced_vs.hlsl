#include "Common.hlsli"

// Slot 1 carries a row-major world matrix (four float4 rows) and a colour per instance.
struct VSIn
{
    float3 pos    : POSITION;
    float3 nrm    : NORMAL;
    float2 uv     : TEXCOORD0;
    float4 w0     : WORLD0;
    float4 w1     : WORLD1;
    float4 w2     : WORLD2;
    float4 w3     : WORLD3;
    float4 icolor : COLOR0;
};

PhongPSIn main(VSIn i)
{
    float4x4 world = float4x4(i.w0, i.w1, i.w2, i.w3);
    PhongPSIn o;
    float4 wp = mul(float4(i.pos, 1.0), world);
    o.wpos = wp.xyz;
    o.pos = mul(wp, gViewProj);
    o.nrm = normalize(mul(i.nrm, (float3x3)world));
    o.uv = i.uv * gMaterial.w;
    o.color = i.icolor;
    return o;
}
