#include "Common.hlsli"
#include "Fish.hlsli"

// Fish swim animation: the body (object-space X, nose at +0.5, tail joint at -0.5, fins beyond)
// is displaced sideways by a travelling wave whose amplitude grows towards the tail, plus a
// steady bend when turning and an extra flick on the tail fin. Normals follow the local slope.

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD0;
};

PhongPSIn main(VSIn i)
{
    float3 p = i.pos;
    float3 n = i.nrm;
    float u = max(0.5 - p.x, 0.0);                   // 0 at the nose, 1 at the tail joint, ~1.4 at the tail tip
    float env = gAnim2.z + (1.0 - gAnim2.z) * u * u;
    float arg = gAnim.z * u - gAnim.x;
    float dz = gAnim.y * env * sin(arg) + gAnim2.x * u * u;
    dz += gAnim2.y * max(u - 1.0, 0.0) * sin(gAnim.z - gAnim.x + 0.6);
    p.z += dz;

    // d(dz)/dx = -d(dz)/du: tilt the normal about Y by the local slope.
    float ddu = gAnim.y * (env * gAnim.z * cos(arg) + 2.0 * (1.0 - gAnim2.z) * u * sin(arg)) + 2.0 * gAnim2.x * u;
    float a = atan(-ddu);
    float s = sin(a), c = cos(a);
    n = float3(n.x * c - n.z * s, n.y, n.x * s + n.z * c);

    PhongPSIn o;
    float4 wp = mul(float4(p, 1.0), gWorld);
    o.wpos = wp.xyz;
    o.pos = mul(wp, gViewProj);
    o.nrm = normalize(mul(n, (float3x3)gWorld));
    o.uv = i.uv;
    o.color = gColor;
    return o;
}
